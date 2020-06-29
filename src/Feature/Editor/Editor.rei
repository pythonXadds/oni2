open EditorCoreTypes;
open Oni_Core;

[@deriving show]
type t;

type pixelPosition = {
  pixelX: float,
  pixelY: float,
};

type scrollbarMetrics = {
  visible: bool,
  thumbSize: int,
  thumbOffset: int,
};

type viewTokens = {
  tokens: list(BufferViewTokenizer.t),
  byteOffset: int,
  characterOffset: int,
};

let create:
  (
    ~wrap: WordWrap.t=?,
    ~font: Service_Font.font,
    ~buffer: EditorBuffer.t,
    unit
  ) =>
  t;
let copy: t => t;

let getId: t => int;
let getBufferId: t => int;
let getTopVisibleLine: t => int;
let getBottomVisibleLine: t => int;
let getLeftVisibleColumn: t => int;
let getLayout:
  (
    ~showLineNumbers: bool,
    ~isMinimapShown: bool,
    ~maxMinimapCharacters: int,
    t
  ) =>
  EditorLayout.t;
let getCharacterUnderCursor: t => option(Uchar.t);
let getCharacterBehindCursor: t => option(Uchar.t);
let getPrimaryCursor: t => Location.t;
let getVisibleView: t => int;
let getTotalHeightInPixels: t => int;
let getTotalWidthInPixels: t => int;
let getVerticalScrollbarMetrics: (t, int) => scrollbarMetrics;
let getHorizontalScrollbarMetrics: (t, int) => scrollbarMetrics;
let getVimCursors: t => list(Vim.Cursor.t);
let setVimCursors: (~cursors: list(Vim.Cursor.t), t) => t;

let getNearestMatchingPair:
  (
    ~location: Location.t,
    ~pairs: list(LanguageConfiguration.BracketPair.t),
    t
  ) =>
  option((Location.t, Location.t));

let visiblePixelWidth: t => int;
let visiblePixelHeight: t => int;

let font: t => Service_Font.font;

let viewTokens:
  (~line: int, ~position: int, ~colorizer: BufferLineColorizer.t, t) =>
  viewTokens;

let scrollX: t => float;
let scrollY: t => float;
let minimapScrollY: t => float;

let lineHeightInPixels: t => float;
let characterWidthInPixels: t => float;

let selection: t => VisualRange.t;
let setSelection: (~selection: VisualRange.t, t) => t;
let selectionOrCursorRange: t => Range.t;

let totalViewLines: t => int;

let scrollToColumn: (~column: int, t) => t;
let scrollToPixelX: (~pixelX: float, t) => t;
let scrollDeltaPixelX: (~pixelX: float, t) => t;

let scrollToLine: (~line: int, t) => t;
let scrollToPixelY: (~pixelY: float, t) => t;
let scrollDeltaPixelY: (~pixelY: float, t) => t;

let scrollToPixelXY: (~pixelX: float, ~pixelY: float, t) => t;
let scrollDeltaPixelXY: (~pixelX: float, ~pixelY: float, t) => t;

let getCharacterWidth: t => float;
let getLineHeight: t => float;

// VIEW-SPACE CONVERSION

let viewLineToBufferLine: (~line: int, t) => int;

// PIXEL-SPACE CONVERSION

// These methods convert a buffer (line, byte) or (line, utf8 character index)
// to a pixel position on-screen - accounting for word wrap, folding, scrolling, etc.

// They return both the pixel position, as well as the character width of the target character.
let bufferLineByteToPixel:
  (~line: int, ~byteIndex: int, t) => (pixelPosition, float);
let bufferLineCharacterToPixel:
  (~line: int, ~characterIndex: int, t) => (pixelPosition, float);

// PROJECTION

// Convert (or unconvent) a (line, column) to a different coordinate space

// [project] - given a zero-based [line] and [column], and a pixel space
// defined by [pixelWidth] and [pixelHeight], return the [(pixelX, pixelY)]
// corresponding to the top-left of the line and column.
let project:
  (~line: int, ~column: int, ~pixelWidth: int, ~pixelHeight: int, t) =>
  (float, float);

// [projectLine] - like [project], but ignoring the [column]/[width]
let projectLine: (~line: int, ~pixelHeight: int, t) => float;

// [unproject] - given a pixel space defined by [pixelWidth] and [pixelHeight],
// map the [pixelX] and [pixelY] of that space to a pixel position on the editor
// surface - [(surfacePixelX, surfacePixelY)]

let unprojectToPixel:
  (~pixelX: float, ~pixelY: float, ~pixelWidth: int, ~pixelHeight: int, t) =>
  (float, float);

let setFont: (~font: Service_Font.font, t) => t;
let setSize: (~pixelWidth: int, ~pixelHeight: int, t) => t;

let updateBuffer:
  (~update: Oni_Core.BufferUpdate.t, ~buffer: EditorBuffer.t, t) => t;

module Slow: {
  let pixelPositionToBufferLineByte:
    (~buffer: Buffer.t, ~pixelX: float, ~pixelY: float, t) => (int, int);
};
