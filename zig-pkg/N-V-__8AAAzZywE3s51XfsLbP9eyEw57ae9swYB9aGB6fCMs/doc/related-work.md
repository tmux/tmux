# Related Work

Wuffs is an industrial project, not an academic project, and this Related Work
section is not as extensive or as rigorous as would be found in an academic
thesis or journal article. The underlying goal is usefulness, not novelty. For
Wuffs' users, it'd be perfectly fine to have multiple implementations of a good
idea, even if the later ones wouldn't earn a Ph. D.

A number of other projects are listed below. Wuffs is not exactly like any of
them, for one reason or another. We're not claiming that "use a state of the
art theorem prover" or "write it in Rust" are unworkable approaches. They're
just different approaches, with different trade-offs.

For example, Wuffs is primarily concerned about *safety* and *performance*.
Formal *correctness*, such as being able to mathematically express that "when
this function returns, that array's elements will be sorted in increasing
order", is less of a concern for Wuffs the Language than for other projects.
This is especially as higher level concerns like "correctly implements the JPEG
specification" are less amenable to formalization than foundational concepts
like searching and sorting. Even with something as mathematical and
conceptually simple as Quicksort, compare a two-line [Haskell
implementation](https://rosettacode.org/wiki/Sorting_algorithms/Quicksort#Haskell)
with the essay that is an [Idris
implementation](https://github.com/bmsherman/blog/wiki/Quicksort-in-Idris).
Proving *correctness* is an admirable goal, but it is not Wuffs' goal.

For Wuffs the Library, confidence about correctness is instead based on
testing, both unit testing and for e.g. media codecs, corpora of test files. A
small corpus is included in the Wuffs source code repository. Other, much
larger corpora (e.g. based on a sample of a web crawl), are not included for
download size and copyright reasons.


## Static Checkers

Wuffs is a language by itself, integrated with a compiler, not something
embedded in the comments of another language's program, supported by a separate
program checker, as the bounds check elimination affects the generated code.

[Checked C](https://www.microsoft.com/en-us/research/project/checked-c/) has a
similar goal of safety, but aims to be backwards compatible with C, including
the idiomatic ways in which C code plays fast and loose with pointers and
arrays, and implicitly inserting run time checks while maintaining existing
data layout (e.g. the sizeof a struct type) for interoperability with other
systems. This is a much harder problem than Wuffs' approach of a new, simple,
domain-specific programming language.

Extended Static Checker for Java (ESC/Java) and its successor
[OpenJML](http://www.openjml.org/), which obviously target the Java programming
language, similarly has to analyze a language that is more complicated than
Wuffs. Java is also not commonly used for writing e.g. low level image codecs,
as the language lacks unsigned integer types, it is garbage collected and
idiomatic code often allocates.

Similarly, [Whiley](http://whiley.org/) is a programming language with extended
static checking. In contrast to Wuffs, its `int` type uses unbounded
arithmetic, instead of e.g. 32-bit twos-complement representation. That can be
easier for theorem provers to reason about, but this can have a performance
impact when such integers are used in inner loops. In theory, a compiler could
recognize "an `int` restricted to the range [0, 99]" as a `uint8` instead of a
potentially 'big integer', but even so, it may improve performance to
explicitly use a `uint32` here even when a `uint8` would do.

[Spark ADA](http://libre.adacore.com/tools/spark-gpl-edition/) targets a subset
of the Ada programming language, which again is more complicated than Wuffs. It
also allows for pluggable theorem provers such as Z3, similar to
[Dafny](http://research.microsoft.com/dafny). This can increase programmer
productivity in that it can take less effort to prove more programs safe, but
it also affects reproducibility (when some programs that are provable in one
programmer's configuration are unprovable in another) and the size of the
trusted computing base. It's also not obvious up front what effect different
theorem provers, and their different heuristics, will have on compile times or
proof times.

In constrast, Wuffs' proof system is fast (and dumb) instead of smart (and
slow). For example, [Vale (Verified Assembly Language for
Everest)](https://github.com/project-everest/vale) is a promising approach that
uses Dafny to verify implementations of cryptographic algorithms. However,
["Vale: Verifying High-Performance Cryptographic Assembly
Code"](http://www.andrew.cmu.edu/user/bparno/papers/vale.pdf) reports in Table
1 that verification times are measured in *minutes*. Wuffs aims for
*sub-second* compilation times, including proofs of (memory) safety. Figure 15
in that paper suggests that Vale's verification times can grow exponentially in
some cases, and also goes on to say, "Dafny/Z3 fails to verify the AES key
inversion for 6 unrolled iterations and 9 unrolled iterations, indicating that
SMT solvers like Z3 are still occasionally unpredictable". Of course, Vale is
also tackling a much harder problem than Wuffs: proving correctness, not just
safety.

Once again, we're not claiming that these other approaches are unworkable, or
not useful, just different, with different trade-offs.


## Why a New Language?

Simpler languages are easier to prove things about. Macros, inheritance,
closures, generics, operator overloading, goto's and built-in container types
are all useful features, but as mentioned in the top-level
[README](/README.md), Wuffs is not a general purpose programming language.
Instead, its focus is on compile-time provable memory safety, with C-like
performance, for CPU-intensive file format decoders.

Nonetheless, Wuffs is an imperative language, not a functional language,
despite the long history of functional languages and provability. Inner loop
performance usage matters, and it is easier to match the optimization
techniques of incumbent C libraries with a C-like language than with a
functional language. Compared to something like
[F\*](https://www.fstar-lang.org/), [Idris](https://www.idris-lang.org/) or
[Liquid Haskell](https://ucsd-progsys.github.io/liquidhaskell-blog/), Wuffs has
no garbage collector overhead, as it has no garbage collector.

Memory usage also matters, again considering per-pixel costs can be multiplied
by millions of pixels. It is important to represent e.g. an RGBA pixel as four
u8 values (or one u32), not as four naturally-sized-for-the-CPU integers or
four 'big integers'.

[F\* / KreMLin](https://github.com/FStarLang/kremlin) is a subset of F\* that
is similar in some sense to Wuffs, in that it transpiles to C and cares about
both safety and performance. Unlike Wuffs, it is a functional language (with
dependent types), not an imperative one (with a simpler type system), and uses
a sophisticated theorem prover like Z3, with the same trade-offs as discussed
above for Spark ADA and Dafny. It also tackles a more complicated problem, in
attempting to prove correctness properties, not just safety properties. Further
reading is in ["Everest: Towards a Verified, Drop-in Replacement of
HTTPS"](https://project-everest.github.io/assets/snapl2017.pdf) and ["Verified
Low-Level Programming Embedded in F\*"](https://arxiv.org/pdf/1703.00053.pdf).

[Cryptol](https://github.com/GaloisInc/cryptol) is another project that, like
Everest (including F\* / KreMLin and its HACL\* sub-project, and Dafny / Vale),
focuses on cryptographic algorithms, rather than Wuffs' focus on file formats,
and also relies on a sophisticated theorem prover like Z3.

Once again, we're not claiming that these other approaches are unworkable, or
not useful, just different, with different trade-offs.


## Why Not ATS?

ATS (Applied Type System) is a statically typed programming language that
unifies implementation with formal specification. Its goals sound similar to
Wuffs in the abstract, but they differ in their approach. That's not to say
that one is better or worse than the other, they're just different.

Graydon Hoare, on the topic of ["Extended static checking (ESC), refinement
types, general dependent-typed
languages"](https://graydon2.dreamwidth.org/253769.html) says of existing
approaches, including ATS, "So far, most exercises in this space have ended in
frustration... the complexity wall here can make other areas of computering
[sic] look... a bit like child's play. It gets very dense, very fast."

In contrast, Wuffs aims to be significantly simpler. The trade-off is, of
course, that Wuffs is not and does not aim to be a general purpose language.
ATS might be more powerful and therefore placed higher in the [Blub
Paradox](http://www.paulgraham.com/avg.html) analogy, but the flip side of that
is that, unlike Lisp, ATS is much more complicated to learn and to understand,
perhaps dauntingly so. For example, [its
manual](http://ats-lang.sourceforge.net/DOCUMENT/INT2PROGINATS/HTML/book1.html)
mentions that types, sorts, views, viewtypes, dataviews, datatypes and
dataviewtypes are all similar-sounding but distinct ATS concepts. As of
February 2018, the [ATS home page](http://www.ats-lang.org/) says that "the
current implementation... [consists] of more than 180K lines of code" and that
"in general, one should expect to encounter many unfamiliar programming
concepts and features in ATS and be prepared to spend a substantial amount of
time on learning them." In the [words of one
commentator](https://www.reddit.com/r/rust/comments/7d85sj/puffs_a_new_language_for_parsing_untrusted_files/dpx0hp7/),
"the cognitive overhead of managing your proof-threading in ATS is much higher
than managing your lifetimes/borrows in Rust, which is by far the biggest cliff
of Rust's learning curve. Not to mention ATS is syntactically... challenging".

On verification, [ATS' Examples](http://www.ats-lang.org/Examples.html) says
that "A fully verified implementaion of the [Fibonacci] function in ATS can now
be given... Please click
[here](http://www.ats-lang.org/SERVER/MYCODE/Patsoptaas_serve.php?mycode_fil=fibats)
if you are interested in compiling and running this example on-line." Modifying
that example's "N" value from 10 to 10000 yields "fibats(10000) = Infinity",
which is clearly incorrect. Somewhere along the way, there's been a breakdown
between numbers in the ideal mathematical sense (often used by proof systems)
and numbers in practice (whether fixed width integers or floating point) as
actually computed on by CPUs. Regardless of where that breakdown is in this
case, it doesn't build confidence that, in general, ATS programs are guaranteed
free of arithmetic overflow despite compiler-verified proofs.

Once again, we're not claiming that these other approaches are unworkable, or
not useful, just different, with different trade-offs.


## Why Not Rust?

Rust is a general purpose programming language, which aims for C-like
performance in general. Wuffs aims for C-like performance specifically for the
limited problem space of bits-and-bytes level CPU-intensive computation (while
doing that safely). As one data point, a [GIF decoding
benchmark](https://github.com/google/wuffs/commit/9a463d46) measures Wuffs as
4x faster than two separate Rust implementations, including the one that
[crates.io calls "gif"](https://crates.io/crates/gif), and one based on Nom
(discussed below).

Wuffs also transpiles to standard C99 code with no dependencies, and should run
anywhere that has a C99 compliant C compiler. An existing C/C++ project, such
as the Chromium web browser, can choose to replace e.g. libpng with Wuffs PNG,
without needing any additional toolchains. Sure, languages like D and Rust have
great binary-level interop with C code, and Mozilla are reporting progress with
parsing media formats in Rust, but it's still an operational hurdle to grow a
project's build process and to assess build times and binary sizes.

[Nom](https://github.com/Geal/nom) is a parser combinator library in Rust,
described in ["Writing parsers like it is
2017"](http://spw17.langsec.org/papers/chifflier-parsing-in-2017.pdf). Wuffs
differs from nom by itself, in that Wuffs is an end to end implementation, not
limited to that part of a file format that is easily expressible as a formal
grammar. In particular, it also handles entropy encodings such as LZW (for
GIF), ZLIB (for PNG) and Huffman (for JPEG, TODO). Wuffs differs from nom
combined with other Rust code (e.g. a Rust LZW implementation) in that bounds
and overflow checks are not just ubiquitous but also completely eliminated at
compile time.

[Kaitai Struct](http://kaitai.io/) is in a similar space, generating safe
parsers for multiple target programming languages from one declarative
specification. Again, Wuffs differs in that it is a complete (and performant)
end to end implementation, not just for the structured parts of a file format.
Repeating a point in the previous paragraph, the difficulty in decoding the GIF
format isn't in the regularly-expressible part of the format, it's in the LZW
compression. [Kaitai's GIF parser](http://formats.kaitai.io/gif/index.html)
returns the compressed LZW data as an opaque blob.

Once again, we're not claiming that these other approaches are unworkable, or
not useful, just different, with different trade-offs.


---

Updated on February 2018.
