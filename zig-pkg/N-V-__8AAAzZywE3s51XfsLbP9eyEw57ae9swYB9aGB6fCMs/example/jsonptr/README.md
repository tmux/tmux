# jsonptr

This program has two goals:

1. be a production-quality [JSON](https://json.org/) formatter (pretty-printer)
   that supports the JSON Pointer (RFC 6901) query syntax.
2. demonstrate how to use [Wuffs' low-level JSON parsing C
   API](https://nigeltao.github.io/blog/2020/jsonptr.html). An alternative,
   higher-level C++ API is used in the sibling
   [example/jsonfindptrs](/example/jsonfindptrs) program.


## Building

It has no dependencies other than Wuffs.

    $ gcc -O3 jsonptr.cc -o jsonptr


## Example

    $ grep foo                ../../test/data/rfc-6901-json-pointer.json
       "foo": ["bar", "baz"],
    $ ./jsonptr -query=/foo/1 ../../test/data/rfc-6901-json-pointer.json
    "baz"

It also supports the JWCC ([JSON With Commas and
Comments](https://nigeltao.github.io/blog/2021/json-with-commas-comments.html))
syntax.

    $ echo '/* Note the commas. */[1,2,[30,31]]' | ./jsonptr -jwcc
    /* Note the commas. */
    [
        1,
        2,
        [
            30,
            31,
        ],
    ]


## Usage

    $ ./jsonptr -help
    Usage: jsonptr -flags input.json
    
    Flags:
        -c      -compact-output
        -d=NUM  -max-output-depth=NUM
        -q=STR  -query=STR
        -s=NUM  -spaces=NUM
        -t      -tabs
                -fail-if-unsandboxed
                -input-allow-comments
                -input-allow-extra-comma
                -input-allow-inf-nan-numbers
                -input-jwcc
                -jwcc
                -output-comments
                -output-extra-comma
                -output-inf-nan-numbers
                -strict-json-pointer-syntax
    
    The input.json filename is optional. If absent, it reads from stdin.
    
    ----
    
    jsonptr is a JSON formatter (pretty-printer) that supports the JSON
    Pointer (RFC 6901) query syntax. It reads UTF-8 JSON from stdin and
    writes canonicalized, formatted UTF-8 JSON to stdout.
    
    Canonicalized means that e.g. "abc\u000A\tx\u0177z" is re-written
    as "abc\n\txŷz". It does not sort object keys, nor does it reject
    duplicate keys. Canonicalization does not imply Unicode normalization.
    
    Formatted means that arrays' and objects' elements are indented, each
    on its own line. Configure this with the -c / -compact-output, -s=NUM /
    -spaces=NUM (for NUM ranging from 0 to 8) and -t / -tabs flags.
    
    The -input-allow-comments flag allows "/*slash-star*/" and
    "//slash-slash" C-style comments within JSON input. Such comments are
    stripped from the output unless -output-comments was also set.
    
    The -input-allow-extra-comma flag allows input like "[1,2,]", with a
    comma after the final element of a JSON list or dictionary.
    
    The -input-allow-inf-nan-numbers flag allows non-finite floating point
    numbers (infinities and not-a-numbers) within JSON input. This flag
    requires that -output-inf-nan-numbers also be set.
    
    The -output-comments flag copies any input comments to the output. It
    has no effect unless -input-allow-comments was also set. Comments look
    better after commas than before them, but a closing "]" or "}" can
    occur after arbitrarily many comments, so -output-comments also requires
    that one or both of -compact-output and -output-extra-comma be set.
    
    With -output-comments, consecutive blank lines collapse to a single
    blank line. Without that flag, all blank lines are removed.
    
    The -output-extra-comma flag writes output like "[1,2,]", with a comma
    after the final element of a JSON list or dictionary. Such commas are
    non-compliant with the JSON specification but many parsers accept them
    and they can produce simpler line-based diffs. This flag is ignored when
    -compact-output is set.
    
    Combining some of those flags results in speaking JWCC (JSON With Commas
    and Comments), not plain JSON. For convenience, the -input-jwcc or -jwcc
    flags enables the first two or all four of:
                -input-allow-comments
                -input-allow-extra-comma
                -output-comments
                -output-extra-comma
    
    ----
    
    The -q=STR or -query=STR flag gives an optional JSON Pointer query, to
    print a subset of the input. For example, given RFC 6901 section 5's
    sample input (https://tools.ietf.org/rfc/rfc6901.txt), this command:
        jsonptr -query=/foo/1 rfc-6901-json-pointer.json
    will print:
        "baz"
    
    An absent query is equivalent to the empty query, which identifies the
    entire input (the root value). Unlike a file system, the "/" query
    does not identify the root. Instead, "" is the root and "/" is the
    child (the value in a key-value pair) of the root whose key is the empty
    string. Similarly, "/xyz" and "/xyz/" are two different nodes.
    
    If the query found a valid JSON value, this program will return a zero
    exit code even if the rest of the input isn't valid JSON. If the query
    did not find a value, or found an invalid one, this program returns a
    non-zero exit code, but may still print partial output to stdout.
    
    The JSON specification (https://json.org/) permits implementations that
    allow duplicate keys, as this one does. This JSON Pointer implementation
    is also greedy, following the first match for each fragment without
    back-tracking. For example, the "/foo/bar" query will fail if the root
    object has multiple "foo" children but the first one doesn't have a
    "bar" child, even if later ones do.
    
    The -strict-json-pointer-syntax flag restricts the -query=STR string to
    exactly RFC 6901, with only two escape sequences: "~0" and "~1" for
    "~" and "/". Without this flag, this program also lets "~n",
    "~r" and "~t" escape the New Line, Carriage Return and Horizontal
    Tab ASCII control characters, which can work better with line oriented
    (and tab separated) Unix tools that assume exactly one record (e.g. one
    JSON Pointer string) per line.
    
    ----
    
    The -d=NUM or -max-output-depth=NUM flag gives the maximum (inclusive)
    output depth. JSON containers ([] arrays and {} objects) can hold other
    containers. When this flag is set, containers at depth NUM are replaced
    with "[…]" or "{…}". A bare -d or -max-output-depth is equivalent to
    -d=1. The flag's absence is equivalent to an unlimited output depth.
    
    The -max-output-depth flag only affects the program's output. It doesn't
    affect whether or not the input is considered valid JSON. The JSON
    specification permits implementations to set their own maximum input
    depth. This JSON implementation sets it to 1024.
    
    Depth is measured in terms of nested containers. It is unaffected by the
    number of spaces or tabs used to indent.
    
    When both -max-output-depth and -query are set, the output depth is
    measured from when the query resolves, not from the input root. The
    input depth (measured from the root) is still limited to 1024.
    
    ----
    
    The -fail-if-unsandboxed flag causes the program to exit if it does not
    self-impose a sandbox. On Linux, it self-imposes a SECCOMP_MODE_STRICT
    sandbox, regardless of whether this flag was set.
