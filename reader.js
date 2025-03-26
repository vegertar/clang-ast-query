import {
  EditorView,
  Decoration,
  lineNumbers,
  highlightActiveLineGutter,
  highlightActiveLine,
  hoverTooltip,
} from "https://cdn.jsdelivr.net/npm/@codemirror/view@6.29.0/+esm";
import {
  Text,
  EditorState,
  StateField,
  RangeSetBuilder,
  RangeValue as BaseRangeValue,
} from "https://cdn.jsdelivr.net/npm/@codemirror/state@6.4.1/+esm";
import isEqual from "https://cdn.jsdelivr.net/npm/lodash.isequal@4.5.0/+esm";

/**
 * @typedef Config
 * @type {{
 *   id?: string,
 *   parent?: Element | DocumentFragment,
 * }}
 */

export class ReaderView {
  /**
   *
   * @param {Config} config
   */
  constructor(config) {
    const { id = getDefaultId(), parent = getDefaultParent() } = config || {};
    this.editor = new EditorView({
      parent,
      doc: Text.of(getData(id, "source")),
      extensions: [
        EditorState.readOnly.of(true),
        // Warn the user that there is some code unstyled.
        EditorView.baseTheme({
          ".cm-line": {
            color: "yellow",
          },
        }),
        lineNumbers(),
        highlightActiveLineGutter(),
        highlightActiveLine(),
        link(getData(id, "link")),
        // decl(data),
        // macroDecl(data),
        // testMacroDecl(data),
        semantics(getData(id, "semantics")),
      ],
    });
  }
}

class RangeValue extends BaseRangeValue {
  #eq;

  constructor(value, eq = isEqual) {
    super();
    this.value = value;
    this.#eq = eq;
  }

  eq(other) {
    return this.#eq(this.value, other.value);
  }
}

/**
 *
 * @returns {string | undefined}
 */
function getDefaultId() {
  return document.querySelector("script[data-main]")?.dataset.id;
}

function getDefaultParent() {
  return document.body.firstElementChild || document.body;
}

/**
 *
 * @param {string} id
 * @param {string} type
 * @returns {any[]}
 */
function getData(id, type) {
  const node = document.querySelector(
    `script[data-id="${id}"][data-type="${type}"]`
  );
  return node?.data || [];
}

/**
 *
 * @param {any[]} data
 * @returns
 */
function link(data) {
  return StateField.define({
    create({ doc }) {
      const builder = new RangeSetBuilder();

      for (let j = 0, n = data.length; j < n; ) {
        const beginRow = data[j++];
        const beginCol = data[j++];
        const endRow = data[j++];
        const endCol = data[j++];
        const file = data[j++];

        const from = doc.line(beginRow).from + beginCol - 1;
        const to = doc.line(endRow).from + endCol - 1;

        builder.add(from, to, new RangeValue(file));
      }
      return builder.finish();
    },
    update(value, tr) {
      return value;
    },
    provide: (f) => [
      hoverTooltip((view, pos, side) => {
        let tooltip;
        view.state.field(f).between(pos, pos, (from, to, { value }) => {
          tooltip = {
            pos: from,
            end: to,
            create(view) {
              const dom = document.createElement("div");
              dom.className = "link";
              const file = dom.appendChild(document.createElement("span"));
              file.className = "file";
              file.textContent = value;
              return { dom };
            },
          };
        });
        return tooltip;
      }),
      EditorView.baseTheme({
        ".link": {
          fontFamily: "monospace",
          margin: "5px",
        },

        ".link::before": {
          color: "#808080",
          fontStyle: "italic",
          content: `"header "`,
        },
        ".link .file": {
          color: "#A31515",
          fontWeight: "bold",
          quotes: `'"' '"'`,
        },
        ".link .file::before": {
          content: "open-quote",
        },
        ".link .file::after": {
          content: "close-quote",
        },
      }),
    ],
  });
}

/**
 *
 * @param {any[]} data
 * @returns
 */
function decl(data) {
  if (!validate(data, "decl")) return nul;

  const first = ++i;
  while (validate(data)) i += 8;
  const last = i;

  return StateField.define({
    create({ doc }) {
      const builder = new RangeSetBuilder();

      for (let j = first; j < last; ) {
        const tokBeginRow = data[j++];
        const tokBeginCol = data[j++];
        const name = data[j++];
        const kind = data[j++];
        const specs = data[j++];
        const elaboratedType = data[j++];
        const qualifiedType = data[j++];
        const desugaredType = data[j++];

        const from = doc.line(tokBeginRow).from + tokBeginCol - 1;
        const to = from + name.length;

        builder.add(
          from,
          to,
          new RangeValue({
            name,
            kind,
            specs,
            elaboratedType,
            qualifiedType,
            desugaredType,
          })
        );
      }
      return builder.finish();
    },
    update(value, tr) {
      return value;
    },
    provide: (f) => [
      hoverTooltip((view, pos, side) => {
        const specNames = {
          1: "extern",
          2: "static",
          4: "inline",
          8: "const",
          16: "volatile",
        };
        const elaboratedNames = ["", "struct", "union", "enum"];

        let tooltip;
        view.state
          .field(f)
          .between(
            pos,
            pos,
            (
              from,
              to,
              {
                value: {
                  name,
                  kind,
                  specs,
                  elaboratedType,
                  qualifiedType,
                  desugaredType,
                },
              }
            ) => {
              tooltip = {
                pos: from,
                end: to,
                create(view) {
                  const dom = document.createElement("div");
                  dom.className = "decl";

                  const sp = dom.appendChild(document.createElement("span"));
                  sp.className = "specifier";
                  for (const k in specNames)
                    if (specs & k) sp.append(specNames[k], " ");

                  sp.append(elaboratedNames[elaboratedType]);

                  const id = dom.appendChild(document.createElement("span"));
                  id.className = `identifier ${kind}`;
                  id.textContent = name;

                  dom.append(": ");

                  const t = dom.appendChild(document.createElement("span"));
                  t.className = "type";
                  t.textContent = qualifiedType;

                  if (desugaredType && desugaredType !== qualifiedType) {
                    const t = dom.appendChild(document.createElement("span"));
                    t.className = "desugared type";
                    t.textContent = desugaredType;
                  }

                  return { dom };
                },
              };
            }
          );
        return tooltip;
      }),
      EditorView.baseTheme({
        ".decl": {
          fontFamily: "monospace",
          margin: "5px",
        },

        ".decl .specifier": {
          color: "#0000ff",
        },
        ".decl .specifier::after": {
          content: `" "`,
        },

        ".decl .identifier": {
          fontWeight: "bold",
        },
        // Set common styles for func/var/type tag.
        ".decl .identifier::before": {
          color: "#808080",
          fontStyle: "italic",
          fontWeight: "normal",
        },
        ".FunctionDecl::before": {
          content: `"func "`,
        },
        ".FunctionDecl": {
          color: "#795E26",
        },

        ".VarDecl::before": {
          content: `"var "`,
        },
        ".VarDecl": {
          color: "#001080",
        },

        ".ParmVarDecl::before": {
          content: `"param "`,
        },
        ".ParmVarDecl": {
          color: "#808080",
        },

        ".FieldDecl::before": {
          content: `"field "`,
        },
        ".FieldDecl": {
          color: "#0451a5",
        },

        ".TypedefDecl::before": {
          content: `"type "`,
        },
        ".TypedefDecl": {
          color: "#267f99",
        },

        ".decl .type": {
          color: "#267f99",
        },
        ".decl .desugared.type::before": {
          content: `":"`,
        },
        ".decl .desugared.type": {
          backgroundColor: "#E5EBF1",
          borderRadius: "4px",
        },
      }),
    ],
  });
}

/**
 *
 * @param {any[]} data
 * @returns
 */
function macroDecl(data) {
  if (!validate(data, "macro_decl")) return nul;

  const first = ++i;
  while (validate(data)) i += 5;
  const last = i;

  return StateField.define({
    create({ doc }) {
      const builder = new RangeSetBuilder();

      for (let j = first; j < last; ) {
        const tokBeginRow = data[j++];
        const tokBeginCol = data[j++];
        const name = data[j++];
        const parameters = data[j++];
        const body = data[j++];

        const from = doc.line(tokBeginRow).from + tokBeginCol - 1;
        const to = from + name.length;

        builder.add(from, to, new RangeValue({ name, parameters, body }));
      }
      return builder.finish();
    },
    update(value, tr) {
      return value;
    },
    provide: (f) => [
      hoverTooltip((view, pos, side) => {
        let tooltip;
        view.state
          .field(f)
          .between(
            pos,
            pos,
            (from, to, { value: { name, parameters, body } }) => {
              tooltip = {
                pos: from,
                end: to,
                create(view) {
                  const dom = document.createElement("div");
                  dom.className = "macro_decl";
                  const id = dom.appendChild(document.createElement("span"));
                  id.className = "identifier";
                  id.textContent = name;
                  if (parameters) dom.append(parameters);
                  if (body) {
                    dom.append(" ");
                    const code = dom.appendChild(
                      document.createElement("code")
                    );
                    code.textContent = body;
                  }
                  return { dom };
                },
              };
            }
          );
        return tooltip;
      }),
      EditorView.baseTheme({
        ".macro_decl": {
          fontFamily: "monospace",
          margin: "5px",
        },

        ".macro_decl::before": {
          color: "#808080",
          fontStyle: "italic",
          content: `"macro "`,
        },

        ".macro_decl .identifier": {
          fontWeight: "bold",
          color: "#0000ff",
        },

        ".macro_decl code": {
          whiteSpace: "pre",
        },
      }),
    ],
  });
}

/**
 *
 * @param {any[]} data
 * @returns
 */
function semantics(data) {
  return StateField.define({
    create({ doc }) {
      const ranges = [];
      for (let j = 0, n = data.length; j < n; ) {
        const beginRow = data[j++];
        const beginCol = data[j++];
        const endRow = data[j++];
        const endCol = data[j++];
        const kind = data[j++];
        const name = data[j++];

        const from = doc.line(beginRow).from + beginCol - 1;
        const to = doc.line(endRow).from + endCol - 1;

        ranges.push(
          Decoration.mark({ class: `semantics ${kind} ${name}` }).range(
            from,
            to
          )
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
        ".KEYWORD.return": {
          color: "#8F08C4",
        },
        ".KEYWORD.for": {
          color: "#8F08C4",
        },
        ".KEYWORD.while": {
          color: "#8F08C4",
        },
        ".KEYWORD.goto": {
          color: "#8F08C4",
        },
        ".KEYWORD.continue": {
          color: "#8F08C4",
        },
        ".KEYWORD.break": {
          color: "#8F08C4",
        },
        ".KEYWORD.switch": {
          color: "#8F08C4",
        },
        ".KEYWORD.case": {
          color: "#8F08C4",
        },
        ".KEYWORD.default": {
          color: "#8F08C4",
        },

        ".PPKEYWORD": {
          color: "#808080",
        },

        ".LITERAL": {
          color: "#A31515",
        },
        ".numeric_constant": {
          color: "#098658",
        },
        ".char_constant": {
          color: "#0000ff",
        },

        ".INACTIVE": {
          color: "#E5EBF1",
        },

        ".COMMENT": {
          color: "#008000",
        },

        ".IDENTIFIER": {
          color: "#000000",
        },
        ".macro": {
          color: "#0000ff",
        },
        ".function_like_macro": {
          color: "#8A1BFF",
        },
        ".Function": {
          color: "#795E26",
        },
        ".Var": {
          color: "#001080",
        },
        ".ParmVar": {
          color: "#808080",
        },
        ".Field": {
          color: "#0451a5",
        },
        ".Typedef": {
          color: "#267f99",
        },

        ".PUNCTUATION": {
          color: "#A31515",
        },

        ".TOKEN": {
          color: "#000000",
        },
        ".header_name": {
          color: "#a31515",
          textDecoration: "underline 1px",
        },

        ".EXPANSION": {
          textDecorationStyle: "dotted !important",
          textDecoration: "underline 1px",
        },
      }),
    ],
  });
}

const testMacroDecl = macroDecl;
