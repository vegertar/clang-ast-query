import {
  EditorView,
  Decoration,
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
      extensions: [EditorState.readOnly.of(true), link(data)],
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
}
