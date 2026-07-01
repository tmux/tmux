// Copyright 2018 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

//go:build ignore
// +build ignore

package main

// crawl.go crawls a list of HTTP and HTTPS URLs. If the URL yields an HTML
// file, that file is parsed and the "<img src=etc>" tags within it are
// followed (but not recursively).
//
// The crawler writes files to disk with filenames based on the hash of the
// content (thus de-duplicating e.g. a site's 404 Not Found page even if served
// from multiple URLs). It also writes a manifest.tsv file that records the
// mapping from the original URL to that content hash.
//
// Usage: go run crawl.go -outdir foo -infile urls.txt 2>log.txt

import (
	"bufio"
	"bytes"
	"crypto/sha256"
	"flag"
	"fmt"
	"io"
	"log"
	"math/rand"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"sync"
	"time"

	"golang.org/x/net/html"
)

var (
	datadirlevelsFlag = flag.Int("datadirlevels", 1,
		"number of directories in the ab/cd/efgh output data filenames; valid range is [0..8]")
	flushfreqFlag   = flag.Int("flushfreq", 256, "write out in-progress manifest.tsv on every flushfreq entries")
	httptimeoutFlag = flag.Duration("httptimeout", 30*time.Second, "HTTP Client timeout; zero means unlimited")
	infileFlag      = flag.String("infile", "", "source file containing URLs to crawl")
	inlimitFlag     = flag.Int("inlimit", -1, "if non-negative, the maximum number of input URLs")
	nworkersFlag    = flag.Int("nworkers", 16, "number of concurrent crawler workers")
	outdirFlag      = flag.String("outdir", "", "directory to place crawled data")
	sleepFlag       = flag.Duration("sleep", 1*time.Second,
		"minimum duration to sleep between HTTP requests to the same domain")
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	flag.Parse()
	if *datadirlevelsFlag < 0 || 8 < *datadirlevelsFlag {
		return fmt.Errorf("-datadirlevels out of bounds [0..8]")
	}
	if *infileFlag == "" {
		return fmt.Errorf("-infile not given")
	}
	if *outdirFlag == "" {
		return fmt.Errorf("-outdir not given")
	}

	urlsGroupedByDomain, err := parseInfile()
	if err != nil {
		return err
	}

	global.manifest, err = parseManifest()
	if err != nil {
		return err
	}

	wg := sync.WaitGroup{}
	urlsChan := make(chan []*url.URL)
	for i := 0; i < *nworkersFlag; i++ {
		wg.Add(1)
		w := worker(i)
		go w.work(&wg, urlsChan)
	}
	for _, u := range urlsGroupedByDomain {
		urlsChan <- u
	}
	close(urlsChan)
	log.Printf("controller: no more work (inlimit is %d)", *inlimitFlag)
	wg.Wait()

	return flush(global.manifest.m)
}

var global struct {
	manifest manifest
}

const keySize = sha256.Size // 32 bytes.

type key [keySize]byte

func (k key) Cmp(l key) int {
	for a := 0; a < keySize; a++ {
		if k[a] < l[a] {
			return -1
		} else if k[a] > l[a] {
			return +1
		}
	}
	return 0
}

func (k key) Str(levels int) string {
	const hex = "0123456789abcdef"
	var b [128]byte
	n := 0
	for i, x := range k {
		b[n+0] = hex[x>>4]
		b[n+1] = hex[x&0xF]
		n += 2
		if i < levels {
			b[n] = '/'
			n++
		}
	}
	return string(b[:n])
}

func hash(b []byte) key {
	return sha256.Sum256(b)
}

func unhex(b byte) uint8 {
	if '0' <= b && b <= '9' {
		return b - '0'
	}
	if 'a' <= b && b <= 'f' {
		return b + 10 - 'a'
	}
	return 0xFF
}

func parseHash(b []byte) (k key, ok bool) {
	i := 0
	for i < keySize && len(b) >= 2 {
		if b[0] == '/' {
			b = b[1:]
			continue
		}
		u0 := unhex(b[0])
		u1 := unhex(b[1])
		if u0 > 15 || u1 > 15 {
			return key{}, false
		}
		k[i] = u0<<4 | u1
		i++
		b = b[2:]
	}
	return k, i == keySize && len(b) == 0
}

type entry struct {
	contentHash     key
	urlHash         key
	httpStatusCode  uint32
	sniffedMimeType string
	size            uint64
	url             *url.URL
}

type manifest struct {
	lock       sync.Mutex
	m          map[key]entry // maps from urlHash to entry.
	numPending int
	flushing   bool
}

func (m *manifest) get(k key) entry {
	m.lock.Lock()
	e := m.m[k]
	m.lock.Unlock()
	return e
}

func (m *manifest) put(k key, e entry) {
	clone := map[key]entry(nil)

	m.lock.Lock()
	if m.m == nil {
		m.m = map[key]entry{}
	}
	m.m[k] = e
	m.numPending++
	if m.numPending >= *flushfreqFlag && !m.flushing {
		m.numPending = 0
		m.flushing = true
		clone = make(map[key]entry, len(m.m))
		for mk, mv := range m.m {
			clone[mk] = mv
		}
	}
	m.lock.Unlock()

	if clone != nil {
		if err := flush(clone); err != nil {
			log.Println(err)
		}

		m.lock.Lock()
		m.flushing = false
		m.lock.Unlock()
	}
}

func flush(m map[key]entry) error {
	log.Printf("Write manifest.tsv (%d entries) started...", len(m))
	defer log.Printf("Write manifest.tsv (%d entries) finished.", len(m))

	keys := make([][2]key, 0, len(m))
	for _, v := range m {
		keys = append(keys, [2]key{v.contentHash, v.urlHash})
	}

	sort.Slice(keys, func(i, j int) bool {
		if cmp := keys[i][0].Cmp(keys[j][0]); cmp != 0 {
			return cmp < 0
		}
		if cmp := keys[i][1].Cmp(keys[j][1]); cmp != 0 {
			return cmp < 0
		}
		return false
	})

	filename0 := filepath.Join(*outdirFlag, "manifest.tsv.inprogress")
	filename1 := filepath.Join(*outdirFlag, "manifest.tsv")

	f, err := os.Create(filename0)
	if err != nil {
		return err
	}

	w := bufio.NewWriter(f)
	fmt.Fprintf(w, "#ContentHash\tURLHash\tHTTPStatusCode\tSniffedMIMEType\tSize\tURL\n")
	for _, k := range keys {
		e := m[k[1]]
		fmt.Fprintf(w, "%s\t%s\t%d\t%s\t%d\t%v\n",
			e.contentHash.Str(*datadirlevelsFlag), e.urlHash.Str(0),
			e.httpStatusCode, e.sniffedMimeType, e.size, e.url)
	}
	if err := w.Flush(); err != nil {
		f.Close()
		return err
	}
	if err := f.Close(); err != nil {
		return err
	}

	return os.Rename(filename0, filename1)
}

func parseInfile() (map[string][]*url.URL, error) {
	f, err := os.Open(*infileFlag)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	m := map[string][]*url.URL{}

	n := 0
	s := bufio.NewScanner(f)
	for s.Scan() {
		b := s.Bytes()

		// Strip leading whitespace (space, tab or other control character).
		for len(b) > 0 && b[0] <= ' ' {
			b = b[1:]
		}

		// Skip empty lines or comments starting with "#".
		if len(b) == 0 || b[0] == '#' {
			continue
		}

		// Strip everything up to the first whitespace.
		for i, x := range b {
			if x <= ' ' {
				b = b[:i]
				break
			}
		}

		u, err := url.Parse(string(b))
		if err != nil {
			continue
		}

		if *inlimitFlag >= 0 && n == *inlimitFlag {
			break
		}
		n++

		m[u.Host] = append(m[u.Host], u)
	}
	if err := s.Err(); err != nil {
		return nil, err
	}

	return m, nil
}

func parseManifest() (manifest, error) {
	f, err := os.Open(filepath.Join(*outdirFlag, "manifest.tsv"))
	if err != nil {
		if os.IsNotExist(err) {
			err = nil
		}
		return manifest{}, err
	}
	defer f.Close()

	m := map[key]entry{}

	s := bufio.NewScanner(f)
	for s.Scan() {
		b := s.Bytes()

		// Strip leading whitespace (space, tab or other control character).
		for len(b) > 0 && b[0] <= ' ' {
			b = b[1:]
		}

		// Skip empty lines or comments starting with "#".
		if len(b) == 0 || b[0] == '#' {
			continue
		}

		e := parseEntry(b)
		if e.url == nil {
			continue
		}

		m[e.urlHash] = e
	}
	if err := s.Err(); err != nil {
		return manifest{}, err
	}

	return manifest{m: m}, nil
}

func parseEntry(b []byte) (e entry) {
	if i := bytes.IndexByte(b, '\t'); i < 0 {
		return entry{}
	} else if h, ok := parseHash(b[:i]); !ok {
		return entry{}
	} else {
		e.contentHash = h
		b = b[i+1:]
	}

	if i := bytes.IndexByte(b, '\t'); i < 0 {
		return entry{}
	} else if h, ok := parseHash(b[:i]); !ok {
		return entry{}
	} else {
		e.urlHash = h
		b = b[i+1:]
	}

	if i := bytes.IndexByte(b, '\t'); i < 0 {
		return entry{}
	} else if u, err := strconv.ParseUint(string(b[:i]), 10, 32); err != nil {
		return entry{}
	} else {
		e.httpStatusCode = uint32(u)
		b = b[i+1:]
	}

	if i := bytes.IndexByte(b, '\t'); i < 0 {
		return entry{}
	} else {
		e.sniffedMimeType = string(b[:i])
		b = b[i+1:]
	}

	if i := bytes.IndexByte(b, '\t'); i < 0 {
		return entry{}
	} else if u, err := strconv.ParseUint(string(b[:i]), 10, 64); err != nil {
		return entry{}
	} else {
		e.size = uint64(u)
		b = b[i+1:]
	}

	if u, err := url.Parse(string(b)); err != nil {
		return entry{}
	} else {
		e.url = u
	}

	return e
}

func sleep(rng *rand.Rand) {
	jitter := rng.Int63n(int64(*sleepFlag))
	time.Sleep(*sleepFlag + time.Duration(jitter))
}

type worker int

func (w worker) work(wg *sync.WaitGroup, urlsChan chan []*url.URL) {
	rng := rand.New(rand.NewSource(time.Now().UnixNano()))
	defer wg.Done()
	for urls := range urlsChan {
		for _, u := range urls {
			e, links, fetched := w.work1(u, true)
			if fetched {
				sleep(rng)
			}

			for _, u := range links {
				if ee, _, fetched := w.work1(u, false); fetched {
					sleep(rng)
					if ee.url != nil {
						global.manifest.put(ee.urlHash, ee)
					}
				}
			}

			if fetched && e.url != nil {
				global.manifest.put(e.urlHash, e)
			}
		}
	}
	log.Printf("worker #%03d: no more work", w)
}

func (w worker) work1(u *url.URL, followHTML bool) (e entry, links []*url.URL, fetched bool) {
	if u.Scheme != "http" && u.Scheme != "https" {
		return entry{}, nil, false
	}

	urlString := u.String()
	urlHash := hash([]byte(urlString))
	e = global.manifest.get(urlHash)
	if e.url != nil {
		log.Printf("worker #%03d: skipping %q", w, urlString)
		return e, nil, false
	}
	log.Printf("worker #%03d: fetching %q", w, urlString)

	statusCode, data, err := fetch(urlString)
	if err != nil {
		log.Printf("worker #%03d: %v", w, err)
		return entry{}, nil, true
	}
	e = entry{
		contentHash:     hash(data),
		urlHash:         urlHash,
		httpStatusCode:  statusCode,
		sniffedMimeType: sniff(data),
		size:            uint64(len(data)),
		url:             u,
	}

	filename := filepath.Join(*outdirFlag, "data", filepath.FromSlash(e.contentHash.Str(*datadirlevelsFlag)))
	if _, err := os.Stat(filename); os.IsNotExist(err) {
		log.Printf("worker #%03d: writing %q", w, urlString)
		os.MkdirAll(filepath.Dir(filename), 0755)
		if err := os.WriteFile(filename, data, 0644); err != nil {
			log.Println(err)
			return entry{}, nil, true
		}
	}

	if followHTML && looksLikeHTML(data) {
		log.Printf("worker #%03d: parsing %q", w, urlString)
		links = parseHTML(u, data)
	}
	return e, links, true
}

func fetch(urlString string) (statusCode uint32, body []byte, retErr error) {
	client := &http.Client{
		Timeout: *httptimeoutFlag,
	}

	res, err := client.Get(urlString)
	if err != nil {
		return 0, nil, err
	}
	defer res.Body.Close()

	body, err = io.ReadAll(res.Body)
	if err != nil {
		return 0, nil, err
	}
	return uint32(res.StatusCode), body, nil
}

func parseHTML(u *url.URL, data []byte) (links []*url.URL) {
	z := html.NewTokenizer(bytes.NewReader(data))
	for {
		tt := z.Next()
		if tt == html.ErrorToken {
			break
		}
		if tt != html.StartTagToken && tt != html.SelfClosingTagToken {
			continue
		}
		tn, hasAttr := z.TagName()
		if len(tn) != 3 || string(tn) != "img" {
			continue
		}
		for hasAttr {
			key, val, moreAttr := z.TagAttr()
			if len(key) == 3 && string(key) == "src" {
				if v, err := url.Parse(string(val)); err == nil {
					links = append(links, u.ResolveReference(v))
				}
			}
			hasAttr = moreAttr
		}
	}
	return links
}

var (
	prefixBM     = []byte("BM")
	prefixGIF    = []byte("GIF8")
	prefixJPEG   = []byte("\xFF\xD8")
	prefixPNG    = []byte("\x89PNG\r\n\x1A\n")
	prefixRIFF   = []byte("RIFF")
	prefixTIFFBE = []byte("II\x2A\x00")
	prefixTIFFLE = []byte("MM\x00\x2A")
	prefixWEBP   = []byte("WEBP")
	prefixZZZZ   = []byte("\x00\x00\x00\x00")
)

func sniff(b []byte) string {
	switch {
	case bytes.HasPrefix(b, prefixBM):
		if len(b) > 6 && bytes.HasPrefix(b[6:], prefixZZZZ) {
			return "image/bmp"
		}
	case bytes.HasPrefix(b, prefixGIF):
		return "image/gif"
	case bytes.HasPrefix(b, prefixJPEG):
		return "image/jpeg"
	case bytes.HasPrefix(b, prefixPNG):
		return "image/png"
	case bytes.HasPrefix(b, prefixRIFF):
		if len(b) > 8 && bytes.HasPrefix(b[8:], prefixWEBP) {
			return "image/webp"
		}
	case bytes.HasPrefix(b, prefixTIFFBE), bytes.HasPrefix(b, prefixTIFFLE):
		return "image/tiff"
	}

	if looksLikeHTML(b) {
		return "text/html"
	}
	return "unknown"
}

func looksLikeHTML(b []byte) bool {
	for len(b) > 0 {
		if b[0] > ' ' {
			return b[0] == '<'
		}
		b = b[1:]
	}
	return false
}
