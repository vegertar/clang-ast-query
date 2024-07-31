import {
  EditorView,
  Decoration,
  lineNumbers,
  highlightActiveLineGutter,
  highlightActiveLine,
} from "https://cdn.jsdelivr.net/npm/@codemirror/view@6.29.0/+esm";
import {
  EditorState,
  StateField,
} from "https://cdn.jsdelivr.net/npm/@codemirror/state@6.4.1/+esm";

export class ReaderView {
  constructor({ doc, data, parent }) {
    this.editor = new EditorView({
      doc,
      parent,
      extensions: [
        EditorState.readOnly.of(true),
        lineNumbers(),
        highlightActiveLineGutter(),
        highlightActiveLine(),
        extensions.map((f) => f(data)),
      ],
    });
  }
}

var i = 0;
const nul = [];

/**
 *
 * @param {any[]} data
 * @param {string} [s]
 * @returns
 */
function validate(data, s) {
  return s
    ? typeof data[i] === "symbol" && data[i].description === s
    : i < data.length && typeof data[i] !== "symbol";
}

const extensions = [
  /**
   *
   * @param {any[]} data
   * @returns
   */
  function link(data) {
    if (!validate(data, "link")) return nul;

    const first = ++i;
    while (validate(data)) i += 5;
    const last = i;

    return StateField.define({
      create({ doc }) {
        const ranges = [];
        for (let j = first; j < last; ) {
          const beginRow = data[j++];
          const beginCol = data[j++];
          const endRow = data[j++];
          const endCol = data[j++];
          const uri = data[j++];

          const from = doc.line(beginRow).from + beginCol - 1;
          const to = doc.line(endRow).from + endCol - 1;
          ranges.push(Decoration.mark({ class: "link", uri }).range(from, to));
        }
        return Decoration.set(ranges);
      },
      update(value, tr) {
        return value;
      },
      provide: (f) => [
        EditorView.decorations.from(f),
        EditorView.baseTheme({
          ".link": {
            textDecoration: "underline 1px",
          },
        }),
      ],
    });
  },

  /**
   *
   * @param {any[]} data
   * @returns
   */
  function decl(data) {
    if (!validate(data, "decl")) return nul;

    const first = ++i;
    while (validate(data)) i += 4;
    const last = i;

    return StateField.define({
      create({ doc }) {
        const ranges = [];
        for (let j = first; j < last; ) {
          const tokBeginRow = data[j++];
          const tokBeginCol = data[j++];
          const name = data[j++];
          const kind = data[j++];

          const from = doc.line(tokBeginRow).from + tokBeginCol - 1;
          const to = from + name.length;
          ranges.push(
            Decoration.mark({ class: `decl ${kind}` }).range(from, to)
          );
        }
        return Decoration.set(ranges);
      },
      update(value, tr) {
        return value;
      },
      provide: (f) => [
        EditorView.decorations.from(f),
        EditorView.baseTheme({
          ".FunctionDecl": {
            color: "#795E26",
          },
          ".VarDecl": {
            color: "#001080",
          },
          ".ParmVarDecl": {
            color: "#808080",
          },
          ".FieldDecl": {
            color: "#0451a5",
          },
          ".TypedefDecl": {
            color: "#267f99",
          },
          ".ExpansionDecl": {
            color: "#0000ff",
          },
        }),
      ],
    });
  },

  /**
   *
   * @param {any[]} data
   * @returns
   */
  function semantics(data) {
    if (!validate(data, "semantics")) return nul;

    const first = ++i;
    while (validate(data)) i += 5;
    const last = i;

    return StateField.define({
      create({ doc }) {
        const ranges = [];
        for (let j = first; j < last; ) {
          const beginRow = data[j++];
          const beginCol = data[j++];
          const endRow = data[j++];
          const endCol = data[j++];
          const kind = data[j++];

          const from = doc.line(beginRow).from + beginCol - 1;
          const to = doc.line(endRow).from + endCol - 1;

          ranges.push(
            Decoration.mark({ class: `semantics ${kind}` }).range(from, to)
          );
        }

        return Decoration.set(ranges);
      },
      update(value, tr) {
        return value;
      },
      provide: (f) => [
        EditorView.decorations.from(f),
        EditorView.baseTheme({
          ".LITERAL": {
            color: "#A31515",
          },
          ".IDENTIFIER": {
            color: "#000000",
          },
          ".KEYWORD": {
            color: "#0000ff",
          },
          ".KEYWORD.int": {
            color: "#2B91AF",
          },
          ".KEYWORD.long": {
            color: "#2B91AF",
          },
          ".KEYWORD.short": {
            color: "#2B91AF",
          },
          ".KEYWORD.char": {
            color: "#2B91AF",
          },
          ".KEYWORD._Bool": {
            color: "#2B91AF",
          },
          ".KEYWORD.if": {
            color: "#8F08C4",
          },
          ".KEYWORD.else": {
            color: "#8F08C4",
          },
          ".PPKEYWORD": {
            color: "#0000ff",
          },
          ".PPKEYWORD.if": {
            color: "#808080",
          },
          ".PPKEYWORD.else": {
            color: "#808080",
          },
          ".PPKEYWORD.endif": {
            color: "#808080",
          },
          ".PPKEYWORD.ifdef": {
            color: "#808080",
          },
          ".PPKEYWORD.ifndef": {
            color: "#808080",
          },
          ".PPKEYWORD.elifdef": {
            color: "#808080",
          },
          ".PPKEYWORD.elifndef": {
            color: "#808080",
          },
          ".PUNCTUATION": {
            color: "#A31515",
          },
          ".numeric_constant": {
            color: "#098658",
          },
          ".macro": {
            color: "#0000ff",
          },
          ".function_like_macro": {
            color: "#8A1BFF",
          },
          ".EXPANSION": {
            textDecorationStyle: "dotted !important",
            textDecoration: "underline 1px",
          },
          ".INACTIVE": {
            color: "#E5EBF1",
          },
        }),
      ],
    });
  },
];
