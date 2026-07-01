# Hello `wuffs-c`

This directory contains a simple example of a C program using a Wuffs library.
It uses the `wuffs-c` command line tool, which transpiles from Wuffs code to
generate C code.

Traditionally, the first program anyone writes in a given programming language
is something that prints "Hello world". This doesn't work for Wuffs, for two
reasons. One is that Wuffs doesn't have a string type per se. Two is that Wuffs
code doesn't even have the capability to write to files directly, such as to
`stdout`. Wuffs is a language for writing libraries, not complete programs, and
the less Wuffs can do, the less Wuffs can do that is surprising (such as upload
your files to the internet), even when processing untrusted input.

Instead, we're going to run some Wuffs code that parses a string (like `"123"`)
and returns an integer (like `123`). It'll be similar to the C `atoi` function,
although our function will return unsigned instead of signed, as it's a simpler
problem. Our function will also take a pointer-length pair, not just a C style
pointer. Anyway, here's what the output should look like:

```
$ ./run.sh
--- C Implementation Prints ---
0
12
56789
4294967295
0
3197704724
------ Wuffs Impl Prints ------
0
12
56789
4294967295
parse: demo: too large
0
parse: demo: too large
0
```

The [`run.sh`](/hello-wuffs-c/run.sh) script compiles and runs the
[`main.c`](/hello-wuffs-c/main.c) program twice, without and with Wuffs. On
each run, it parses 6 inputs. The first 4 are within the `uint32_t` range and
the last 2 are not. Here's an excerpt of `main.c`:

```
int main(int argc, char* argv) {
  run("0");
  run("12");
  run("56789");
  run("4294967295");  // (1<<32) - 1, aka UINT32_MAX.
  run("4294967296");  // (1<<32).
  run("123456789012");
  return 0;
}
```

The first run (without Wuffs) uses a simple C implementation:

```
uint32_t parse(char* p, size_t n) {
  uint32_t ret = 0;
  for (size_t i = 0; (i < n) && p[i]; i++) {
    ret = (10 * ret) + (p[i] - '0');
  }
  return ret;
}
```

This works for the first 4 inputs, but silently overflows for the last 2. A
subtle point is that, by default, the C compiler accepted this code without any
indication that integer overflow could occur, yet integer overflow can lead to
[serious bugs](https://en.wikipedia.org/wiki/Stagefright_%28bug%29).

Some C compilers, and some compilers for other languages, can optionally insert
run-time checks for integer overflow, but these are typically disabled by
default because of the performance impact. Having these checks enabled for
developer builds are better than nothing, but it still isn't a complete
solution. We don't ship developer builds to our users, and while computer
programmers are better than the average person at e.g. spotting phishing
attempts, that also means that they can be less likely than the average person
to 'test' their developer builds on the malicious input that users encounter.

Anyway, in Wuffs, integer overflow is a mandatory concern. Addressing that
concern takes a little more code, this time in Wuffs:

```
pub func parser.parse?(src: base.io_reader) {
    var c : base.u8
    while true {
        c = args.src.read_u8?()
        if c == 0 {
            return ok
        }
        if (c < 0x30) or (0x39 < c) {  // '0' and '9' are ASCII 0x30 and 0x39.
            return "#not a digit"
        }
        // Rebase from ASCII (0x30 ..= 0x39) to the value (0 ..= 9).
        c -= 0x30

        if this.val < 429_496729 {
            this.val = (10 * this.val) + (c as base.u32)
            continue
        } else if (this.val > 429_496729) or (c > 5) {
            return "#too large"
        }
        // Uncomment this assertion to see what facts are known here.
        // assert false
        this.val = (10 * this.val) + (c as base.u32)
    } endwhile
}
```

Obviously, we could have written the same careful algorithm in C. The point is
that, unlike C, Wuffs doesn't let you forget to consider integer (or buffer)
overflows. This can certainly be annoying in general, but reassuring whenever
the code has to run in security-concious contexts.

Play with the [parse.wuffs](/hello-wuffs-c/parse.wuffs) code and see what sort
of compiler errors you get:

```diff
diff --git a/hello-wuffs-c/parse.wuffs b/hello-wuffs-c/parse.wuffs
index cb207eec..c38dcf88 100644
--- a/hello-wuffs-c/parse.wuffs
+++ b/hello-wuffs-c/parse.wuffs
@@ -35,7 +35,7 @@ pub func parser.parse?(src: base.io_reader) {
                if this.val < 429_496729 {
                        this.val = (10 * this.val) + (c as base.u32)
                        continue
-               } else if (this.val > 429_496729) or (c > 5) {
+               } else if (this.val > 429_496729) {
                        return "#too large"
                }
                // Uncomment this assertion to see what facts are known here.
```

```
$ ./run.sh
check: expression "(10 * this.val) + (c as base.u32)" bounds [4294967290 ..= 4294967299] is not within bounds [0 ..= 4294967295] at stdin:43. Facts:
    c <> -48
    c >= 0
    c <= 9
    this.val >= 429496729
    this.val <= 429496729
```

Note that the `parser.parse` method is a [coroutine](/doc/note/coroutines.md)
and therefore doesn't return the value directly. Instead, the pattern is that
the `parse` method updates the state and the `value` method returns the state.
For more realistic problem domains, the output often isn't just a single
`uint32_t`, and the pattern for e.g. image decoding in Wuffs is that the
`decode` method takes the destination buffer as an additional argument.

That's the end of this "Hello world"-ish introduction. After that, you can read
more documentation about both [Wuffs the Language](/doc/wuffs-the-language.md)
and [Wuffs the Library](/doc/wuffs-the-library.md) (which links to more example
programs).

Finally, there are admittedly a couple of TODOs in this directory, concerning
some rough edges in what should be a simple example. Version 0.2 (December
2019) has prioritized users of Wuffs the Library over users of Wuffs the
Language. A future version should fix that imbalance.
