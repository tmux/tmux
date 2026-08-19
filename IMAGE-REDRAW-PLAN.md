# Image Redraw and Damage Plan

## Purpose

Make image redraws correct and efficient for tiled panes, floating panes,
scrolling, copy-mode selection, pane borders, pane status lines, scrollbars,
and clients using different output protocols.

The intended model is retained: the visible screen is a scene of horizontal
cell spans. A change damages cell-aligned regions, and the compositor redraws
only scene spans intersecting those regions.

Synchronized output may hide unavoidable intermediate terminal updates, but
it must not be required for correctness.

## Current state and recovery point

The last clean, pushed commit on `4902-image-support` is:

```
ec9859b1 sixel: preserve image aspect across client geometries
```

That commit already contains the important persistent model:

- sparse image placements represented by spans attached to grid lines;
- input-protocol semantics and placement ordering;
- source coordinates for cropping placement spans;
- movement, copying, duplication, and removal of image spans with grid data;
- per-client output backends for Kitty, SIXEL, and fallback rendering; and
- cached image/rendering data.

The current uncommitted changes touch eight files and mix several attempted
optimizations. They are known to damage SIXEL in Mintty and cause unnecessary
redraws with floating panes. Do not commit them as the solution.

Before discarding them, save one patch outside the branch for reference:

```
git diff -- image.c regress/image-support.sh screen-redraw.c screen-write.c \
    server-client.c tmux.h tty.c window-copy.c \
    > /tmp/4902-image-redraw-experiment.patch
```

Then restore those tracked files to `ec9859b1` before beginning the new work.
Do this only after the user explicitly approves the rollback.

### Ideas worth retaining, but reimplementing separately

- Redraw damage should be restricted to scene-visible spans.
- Image-related selection tests are useful.
- Floating-pane and pane-status regression scenarios are useful.
- Window chrome must be emitted after pane image/text composition whenever
  the same damaged region contains both.
- A placement's complete canvas may need to be represented even where its
  pixels are transparent; visibility and alpha must then be tested while
  compositing rather than by losing the placement geometry.
- Native scrolling should be selected according to the affected region and
  backend behaviour.

### Changes not worth retaining in their present form

- Promoting an obscured-pane update to `PANE_REDRAW` and then sometimes to
  `CLIENT_REDRAWWINDOW`.
- Repairing images directly from individual `tty_cmd_*` text functions.
- Separate ad-hoc redraw paths for images, selection, scrollbars, borders,
  and floating panes.
- The current `TTY_CTX_IMAGE_SCROLL`, `TTY_CTX_PANE_FULL_WIDTH`, and
  `CLIENT_REDRAWSCROLLBARS` plumbing as a combined patch.
- Assuming an unchanged pane-status string does not need physical redraw
  after some other drawing operation has covered its cells.

## Required invariants

These rules should be written down in code comments and enforced throughout
the implementation.

1. **Input determines semantics.** Whether text removes an image layer, lies
   above it, or lies below it is determined by the protocol received from the
   application. It must not depend on whether the outer client uses Kitty,
   SIXEL, or fallback output.

2. **Output backends encode a scene.** An output backend may choose different
   escape sequences, caches, or cropping, but it must render the same logical
   result.

3. **One compositor owns repair.** No output path may independently erase and
   reconstruct image cells without going through the same scene ordering.

4. **Damage is regional.** Ordinary content changes do not become pane-wide
   or window-wide redraws. Full redraw remains appropriate for attachment,
   resize, geometry changes, terminal reset, or invalid scene geometry.

5. **Damage uses cell coordinates.** Pixel offsets are handled when an image
   backend maps the damaged cells to a source-pixel crop.

6. **Logical state changes before physical output.** Grid text and image spans
   are moved, removed, or inserted first. Rendering then reads the completed
   logical state.

7. **Chrome is part of composition.** Pane borders, pane status lines,
   scrollbars, menus, and floating panes must not be repaired independently in
   an order that allows image output to cover them afterward.

8. **No-image behaviour remains unchanged.** When a damaged region does not
   intersect image data or graphical overlays, existing fast text output and
   native scrolling should remain available.

## Coordinate spaces

Keep conversions explicit. Do not pass ambiguous `x` and `y` values between
layers.

- **Grid coordinates:** include history and belong to a pane's `struct grid`.
- **Pane coordinates:** visible cells inside a pane.
- **Window coordinates:** include pane offsets and floating-pane positions.
- **Client scene coordinates:** window coordinates clipped by the client's
  viewport (`ox`, `oy`, `sx`, and `sy`).
- **Terminal coordinates:** include the client status-line offset and any
  terminal viewport offset.
- **Image source coordinates:** identify cell rows/columns within a placement;
  the backend maps these to source pixels using terminal cell geometry and
  placement pixel offsets.

Store damage in window coordinates. Translate and clip it separately for each
client when drawing, because clients may have different viewports, status-line
positions, cell geometries, and output backends.

## Compartmentalization

Keep image implementation details in the existing image files. The rest of
tmux should know only that some scene cells require image-aware composition.

### `image.c`

Own all protocol-independent image state and composition policy:

- placements, spans, input protocol, z-index, serial ordering, and alpha;
- applying input semantics when text overwrites image cells;
- testing whether a grid rectangle intersects image placements;
- mapping a damaged cell rectangle to ordered image fragments;
- deciding which logical image layers are below or above the text plane;
- generic backend dispatch; and
- public image helpers used by the redraw subsystem.

### `image-sixel.c`

Own only SIXEL-specific output details:

- cached quantized/indexed representations;
- mapping cell crops to pixel and six-pixel-band crops;
- emitting SIXEL fragments;
- alpha and palette encoding; and
- backend-specific reset, erase, and scrolling capabilities.

### `image-kitty.c`

Own only Kitty-specific output details:

- client-side transmission and placement caches;
- placement creation, clipping, movement, and deletion;
- Kitty IDs and geometry; and
- backend-specific reset and scrolling behaviour.

### Other tmux files

- `screen-redraw.c` owns scene construction, damage clipping, and the ordering
  of pane content versus window chrome. It calls generic `image.c` helpers and
  contains no Kitty or SIXEL protocol logic.
- `screen-write.c` updates logical grid state and reports pane/window damage.
  It must not render image fragments or select an output backend.
- `tty.c` owns terminal text and scrolling primitives. It receives an already
  selected native-scroll operation; it must not reconstruct image layers or
  call image repair after individual characters.
- `server-client.c` schedules and drains per-client damage. It must not inspect
  image placements or output protocols.
- `window-copy.c` reports copy-mode damage and copies logical image state using
  generic image helpers; it must not render images itself.
- `tmux.h` exposes only the minimum generic structures and function prototypes
  required across those boundaries.

Prefer one generic call from `screen-redraw.c` into `image.c` over `ENABLE_IMAGES`
branches scattered through tty commands. A build without image support should
retain the existing redraw path with minimal conditional code.

## Damage representation

Add a small regional damage abstraction owned by the redraw subsystem. Do not
add per-cell queues or enlarge `struct grid_cell`.

A damage entry needs:

```
x, y, sx, sy       window-cell rectangle
reason             content, scroll exposure, pane move, chrome, and so on
```

The exact names should follow tmux style. A short list or dynamic array per
client is sufficient. When a new rectangle is added:

- clip it to the window;
- discard empty rectangles;
- merge overlapping or directly adjacent rectangles where that does not make
  the result substantially larger;
- cap the list at a modest number; above the cap, replace it with their union;
- promote to a full redraw only if geometry is invalid or the union covers
  most of the client scene.

A shared window damage list must not be cleared after the first client draws.
Either append translated window rectangles to every affected client, or give
damage events monotonically increasing sequence numbers so each client can
consume them independently. The simpler initial implementation is to append
window-coordinate rectangles to each client currently displaying the window.

Suggested public operations are conceptually:

```
redraw_damage_window(window, x, y, sx, sy, reason)
redraw_damage_pane(window_pane, x, y, sx, sy, reason)
redraw_damage_client(client, x, y, sx, sy, reason)
```

Do not expose image-backend details through these calls.

## Regional scene composition

Extend `screen-redraw.c` so an existing `redraw_scene` can be drawn through a
set of clipping rectangles.

For every damaged line:

1. Intersect the damage rectangle with that line's scene spans.
2. Split only at damage boundaries; do not rebuild the complete scene.
3. Draw intersecting pane content in logical layer order.
4. Draw window chrome intersecting the same rectangle last.

For a pane span, the scene has already selected the topmost visible pane at
that position. Composition inside that pane should then be explicit:

1. image placements below the text plane, such as negative Kitty z-indexes;
2. the text plane, including the requested copy-mode style;
3. surviving temporal SIXEL placements and images above the text plane;
4. pane borders, pane status, scrollbars, menus, and other chrome.

The exact placement order is already represented by `struct image_placement`
and its input protocol, z-index, and serial. Do not derive it from the client's
output backend.

Only the topmost pane content is normally needed because panes are opaque.
Underlying pane content is retained in its own grid and becomes visible when a
floating pane moves, at which time the old floating-pane rectangle is damaged
and recomposed.

### Image fragments

An `image_span` is placement metadata, not a separately stored bitmap. For an
intersection, compute the corresponding source-cell rectangle and ask the
backend to emit that crop from its cached rendering.

- Do not rerun quantization or dithering for every small damage rectangle.
- SIXEL output operates in six-pixel vertical bands. Expand or clip the
  backend operation as required, then ensure any additionally touched cells
  are included in the same composition pass.
- A transparent top image requires lower image layers to be drawn first.
- Kitty placements may be deleted, clipped, or replaced only within the
  damaged placement region.

## Fast text writes

Do not route every ordinary character through a deferred full compositor.

For an application text write:

1. Apply input-protocol damage rules to the logical image spans. For example,
   text written after a SIXEL image removes the affected SIXEL coverage.
2. Test whether the affected cells still intersect any image layer or scene
   overlay requiring composition.
3. If not, retain the existing direct `tty_cmd_cell`/`tty_cmd_cells` path.
4. If they do, suppress conflicting direct terminal repair and enqueue only
   those cells for regional composition.

This decision belongs above the backend-specific tty output functions. Avoid
callbacks such as `image_draw_after_text()` from several `tty_cmd_*` functions;
they reproduce ordering logic in multiple places.

## Selection and copy mode

Copy mode must not mutate the application's logical image placements.

- Treat selection as a temporary tmux UI style on the text plane.
- Follow the chosen policy that selection highlighting does not overwrite
  image-covered cells.
- Mouse motion should damage only cells whose selection state actually
  changed.
- If changed selection cells do not intersect visible images, draw them using
  the normal text fast path.
- If they intersect an image canvas, compose those cells once; do not erase a
  whole image or pane.
- Entering or leaving copy mode should damage the visible pane rectangle once,
  not repeatedly for every unchanged mouse event.

Copy-mode image spans should be copied before its text view is composed, but
this should be implemented through the same logical state and damage API, not
through special tty repair calls.

## Floating panes

### Focus changes

Changing the active pane should damage only cells whose appearance changes:

- affected pane borders;
- pane status cells whose style or contents change; and
- any active-pane indicators.

It must not redraw pane interiors or retransmit unrelated SIXEL images.

### Moving or resizing a floating pane

For an old rectangle `A` and new rectangle `B`:

- invalidate the scene geometry;
- damage `A union B` (represented as a few rectangles rather than necessarily
  one large bounding box);
- rebuild the scene because pane ownership changed;
- recompose only spans intersecting those rectangles.

The old rectangle restores tiled panes and any image fragments that were
previously hidden. The new rectangle draws the floating pane and its chrome.
Multiple overlapping floating panes follow their window z-order.

## Scrolling

Implement scrolling in three stages. Do not begin with the most complicated
native-scroll optimization.

### Stage 1: correct regional redraw

1. Move grid text and image spans using the existing grid operations.
2. Record the pane scroll rectangle and newly exposed rows.
3. Recompose that rectangle from the completed logical scene.

This may still retransmit more image data than desired, but it establishes a
correct regional baseline without pane- or window-wide redraws.

### Stage 2: native scrolling without obstructions

Use the terminal's native scroll operation only when:

- the backend/terminal is known to scroll its image pixels consistently;
- the source and destination rectangle belong to the same visible pane;
- no floating pane, menu, overlay, pane border/status, or reserved scrollbar
  intersects it;
- required vertical and horizontal margins are available and reliable; and
- existing BCE and terminal-size checks permit the operation.

After native scrolling, compose only newly exposed rows and any protocol-sized
boundary expansion.

### Stage 3: mixed native scrolling around obstructions

Derive safe rectangles from visible scene spans:

- a destination cell is native-scrollable only when its source cell and
  destination cell are both visible parts of the same pane;
- coalesce equal horizontal ranges across adjacent rows into rectangles;
- native-scroll those maximal rectangles;
- recompose cells crossing an obstruction boundary and newly exposed cells.

With one floating pane, left and right strips can often scroll over the full
height. Above and below it, cells whose source crosses behind the floating pane
must be recomposed rather than recovered from terminal pixels.

As a simpler optional strategy, native-scroll the full tiled-pane rectangle
and recompose the floating pane's old/current rectangles afterward. This is
correct if all disturbed cells are marked, but may visibly flash without
synchronized output. Prefer safe-rectangle scrolling once correctness is
established.

If rectangle decomposition becomes too fragmented, fall back to regional
composition. It is an optimization, never a separate correctness path.

## Redraw scheduling

Do not turn queued regional damage into `CLIENT_REDRAWWINDOW` merely because
the tty output buffer is nonempty. Preserve the damage list and process it
after output drains.

Use full redraw only for:

- initial attachment;
- client or terminal geometry changes;
- a terminal reset or invalid backend cache;
- layout/viewport changes where rebuilding and clipping regions is not yet
  safe; or
- a damage union large enough that full redraw is cheaper.

Focus changes, selection changes, ordinary scrolling, and moving one floating
pane should remain regional.

## Debugging instrumentation

Add temporary debug logging before optimizing:

- damage reason and window rectangle;
- client clipping and coordinate translation;
- scene generation used;
- native-scroll rectangles selected;
- spans recomposed in each layer phase; and
- image fragments emitted by each backend.

The log should make it possible to answer, for every SIXEL retransmission,
which damage rectangle required it. Remove excessively noisy instrumentation
before the final commit or keep it behind normal verbose logging.

## Tests

Build the tests before each optimization and keep each test focused on one
invariant.

### Automated regressions

1. **No-image baseline:** ordinary output, scrolling, copy mode, floating pane
   focus, borders, status lines, and scrollbars behave unchanged.
2. **Focus only:** changing focus with a SIXEL elsewhere emits no SIXEL DCS and
   updates only the affected border/status cells.
3. **Selection outside image:** emits no image output.
4. **Selection crossing image:** the image remains intact during and after the
   drag; only changed non-image cells are highlighted.
5. **Unobstructed scroll:** logical image spans move with text; where native
   scrolling is supported, no complete SIXEL retransmission occurs.
6. **Scroll with floating pane:** both pane-status lines, borders, scrollbar,
   floating-pane contents, and visible image fragments remain correct.
7. **Move floating pane:** old and new rectangles are restored; unrelated image
   regions are not emitted.
8. **Multiple floating panes:** overlapping z-order remains correct while the
   top pane moves.
9. **Transparent placement:** lower images/text show through correctly, and
   moving/selection does not treat transparency as absent placement geometry.
10. **Cross-backend clients:** the same input scene produces equivalent results
    on Kitty, SIXEL, and fallback clients attached simultaneously.
11. **Pane status top/bottom:** unchanged status contents are physically
    restored if their cells were damaged.
12. **Scrollbar reserved/overlay:** scroll thumb and track remain present and
    images do not overwrite them.

Where possible, capture raw terminal output and assert both presence and
absence of DCS/APC sequences. `capture-pane` alone cannot prove physical output
ordering because it observes logical grid contents rather than terminal image
pixels.

### Manual matrix

Test at least:

- Microsoft Terminal with SIXEL;
- Mintty with SIXEL;
- WezTerm with SIXEL;
- Kitty with Kitty output;
- a Kitty client attached to a session created from a SIXEL client and the
  reverse; and
- a terminal without graphical image support.

For each, test scrolling by application output and mouse wheel, selecting text,
entering/leaving copy mode, changing focus, opening/closing/moving a floating
pane, pane status at top and bottom, and scrollbars.

## Implementation steps and commit boundaries

This must not be implemented as one monolithic change. Each numbered step is a
separate commit, review, and testing gate. Do not begin the next step until the
current step passes its focused tests. If a step exposes a pre-existing bug,
record it before expanding the patch.

### Step 0: archive the experiment and recover the baseline

**Goal:** Begin development from known source rather than repairing the mixed
experimental diff.

**Work:**

- Preserve the current diff and this plan on a clearly named WIP branch.
- Return `4902-image-support` to clean commit `ec9859b1`.
- Build with and without image support.
- Run the existing image regression and ordinary tmux regression suite.

**Gate:** The branch is clean and existing behaviour is reproduced. No product
code is changed in this step.

### Step 1: add focused regressions and tracing

**Goal:** Establish observable failures before changing redraw behaviour.

**Primary files:** `regress/image-support.sh`, temporary debug logging in
`screen-redraw.c` if necessary.

**Work:** Add independent tests for focus-only changes, selection crossing an
image, unobstructed scrolling, scrolling beneath a floating pane, moving a
floating pane, both pane-status lines, and scrollbars. Raw terminal output must
be used where a test needs to prove that SIXEL or Kitty data was or was not
emitted.

**Gate:** Each test either passes on the baseline or has a documented expected
failure. Tests must not depend on timing races or visual screenshots.

**Not included:** No redraw implementation changes.

### Step 2: introduce regional damage scheduling

**Goal:** Represent and preserve small window-coordinate damage rectangles
without changing the rendered result.

**Primary files:** `screen-redraw.c`, `server-client.c`, minimal declarations in
`tmux.h`.

**Work:** Add per-client damage storage, clipping, merging, a complexity cap,
and verbose logging. Existing callers may initially convert the damage to the
same redraw they used before. Pending tty output must retain the region rather
than promote it automatically to a full-window redraw.

**Gate:** No-image and image output remain visually and byte-for-byte equivalent
where practical. Unit-style tests verify rectangle clipping and merging.

**Not included:** No image-layer traversal, selection changes, floating-pane
optimization, or native scrolling.

### Step 3: add a generic image-region composition API

**Goal:** Give the scene compositor one protocol-independent way to render the
image layers intersecting a cell rectangle.

**Primary files:** `image.c`, `image-sixel.c`, `image-kitty.c`, with only generic
prototypes in `tmux.h`.

**Work:**

- In `image.c`, intersect damage with placement spans and preserve logical
  input/z/serial order.
- In `image-sixel.c`, emit a crop from cached quantized data and handle SIXEL
  band alignment.
- In `image-kitty.c`, update/delete/place only the intersecting cached
  placements.
- Define clearly separate below-text and above-text passes.

**Gate:** Rendering a whole image as adjacent damaged rectangles produces the
same final scene as rendering it in one operation. Test transparent and
overlapping placements on both output backends.

**Not included:** No changes to individual tty text commands and no native
scroll optimization.

### Step 4: clip scene composition to damage rectangles

**Goal:** Make `screen-redraw.c` repaint only scene spans intersecting queued
damage.

**Primary files:** `screen-redraw.c`; generic calls into `image.c`.

**Work:** For each damaged line, intersect existing redraw spans, compose the
selected pane's below-text images, text, and above-text images, then draw
intersecting chrome last. Full redraw continues to use the same compositor with
a full-client damage rectangle.

**Gate:** Full and tiled panes redraw correctly; pane borders, pane status,
scrollbars, menus, and images have deterministic output order. No floating-pane
movement or scroll optimization is added yet.

### Step 5: route text and selection damage through the compositor

**Goal:** Repair only cells where text or copy-mode selection interacts with
image layers.

**Primary files:** `screen-write.c`, `window-copy.c`, `screen-redraw.c`, and
logical helpers in `image.c`.

**Work:** Apply input semantics to logical spans first. Preserve direct tty text
output when no image placement intersects the cells. Otherwise enqueue those
cells for composition. Selection changes damage only cells whose selection
state changed and follow the policy of not overwriting image-covered cells.

**Gate:** Dragging selection across an image never damages it, entering/leaving
copy mode restores the same scene, and selection outside images emits no image
data.

**Not included:** Do not add image callbacks to `tty_cmd_cell`,
`tty_cmd_cells`, or `tty_cmd_clearcharacter`.

### Step 6: make floating-pane changes regional

**Goal:** Focus, movement, resize, open, and close operations damage only the
cells whose scene ownership or chrome changed.

**Primary files:** floating-pane/window code that reports geometry changes,
plus `screen-redraw.c`. Image implementation remains in the image files.

**Work:** Focus changes enqueue border/status cells only. Movement and resize
enqueue the old and new pane rectangles and invalidate scene geometry. Regional
composition restores underlying panes/images in the old rectangle and draws
the floating pane in the new rectangle.

**Gate:** Changing focus emits no unrelated SIXEL/APC. Moving a floating pane
does not damage images, either pane-status line, borders, or scrollbars. Test
one and multiple overlapping floating panes.

### Step 7: implement correct regional scrolling without native optimization

**Goal:** Establish correct scrolling using logical state plus regional
composition.

**Primary files:** grid/image state helpers in `image.c`, damage reporting in
`screen-write.c`, composition in `screen-redraw.c`.

**Work:** Move text and image spans first, then damage the scroll rectangle and
compose it. Do not use special image repair from `tty.c`.

**Gate:** Application scrolling, mouse-wheel copy-mode scrolling, partial
scroll regions, scrollbars, and scrolling beneath floating panes are correct on
Kitty, Microsoft Terminal, Mintty, and WezTerm. This step may redraw more than
the final optimized version, but must not redraw outside the scroll rectangle.

### Step 8: enable native scrolling for unobstructed rectangles

**Goal:** Avoid retransmitting image pixels when the terminal can safely move
them.

**Primary files:** scroll planning in `screen-redraw.c`, generic backend
capability in `image.c`, backend facts in `image-sixel.c` and `image-kitty.c`,
terminal primitive execution in `tty.c`.

**Work:** Select native scrolling only when source/destination cells belong to
the same unobstructed pane and terminal margins/backend behaviour permit it.
Compose newly exposed rows afterward. Retain Step 7 as the fallback.

**Gate:** Raw output proves the native scroll is used and a complete image is
not retransmitted. Final output must match Step 7 on every tested backend.

### Step 9: decompose scrolling around floating panes

**Goal:** Combine native scrolling of safe rectangles with composition of
obstructed and boundary rectangles.

**Primary files:** scroll planning and scene-span traversal in
`screen-redraw.c`; no new protocol logic outside the image files.

**Work:** Derive maximal rectangles where both source and destination are
visible cells of the same pane, coalesce adjacent equal ranges, natively scroll
them, and compose everything else. Enforce a fragmentation threshold that
falls back to Step 7.

**Gate:** Scrolling beneath one or several floating panes produces the same
final scene as Step 7, reduces emitted image data, and never loses pane status,
borders, scrollbars, or images.

### Step 10: cleanup, performance checks, and full regression

**Goal:** Remove superseded paths only after all replacements are proven.

**Work:** Remove obsolete redraw helpers and flags introduced or made unused by
the new path. Reduce temporary logging. Check memory growth, rectangle-list
bounds, output byte counts, and behaviour without image support. Run the entire
regression suite and the manual terminal matrix.

**Gate:** All completion criteria below are satisfied. Synchronized output may
improve appearance, but disabling it does not alter final contents.

Do not squash these implementation steps during development. Keeping their
boundaries visible makes it possible to bisect correctness and performance
regressions. They can be reorganized later only after review.

## Completion criteria

The work is complete when:

- Mintty, Microsoft Terminal, WezTerm, and Kitty show no persistent damage;
- focus and selection do not retransmit unrelated images;
- floating-pane movement redraws only old/new affected regions;
- scrolling preserves images, pane borders, both tiled and floating pane
  status lines, and scrollbars;
- input semantics produce the same composition on every output backend;
- no-image performance and behaviour remain unchanged;
- full regression tests pass; and
- synchronized output improves appearance but disabling it never changes the
  final screen contents.
