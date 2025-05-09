// @ts-check

import {
  EditorView,
  Decoration,
  lineNumbers,
  highlightActiveLineGutter,
  highlightActiveLine,
  hoverTooltip,
} from "@codemirror/view";
import {
  Text,
  EditorState,
  StateField,
  RangeSetBuilder,
  RangeValue as BaseRangeValue,
} from "@codemirror/state";
import { linter } from "@codemirror/lint";
import isEqual from "lodash.isequal";
import { GoldenLayout } from "golden-layout";
import styleToCSS from "style-object-to-css-string";

// @ts-ignore
import glBase from "golden-layout/dist/css/goldenlayout-base.css";
// @ts-ignore
import glDarkTheme from "golden-layout/dist/css/themes/goldenlayout-dark-theme.css";
// @ts-ignore
import glLightTheme from "golden-layout/dist/css/themes/goldenlayout-light-theme.css";

/**
 * @typedef {import("golden-layout").GoldenLayout.VirtuableComponent & {
 *  id: number;
 *  path: string;
 *  container: import("golden-layout").ComponentContainer;
 *  editor: import("@codemirror/view").EditorView;
 * }} EditorComponent
 */

/**
 * @typedef {"source"|"link"|"semantics"|"lint"} ScriptType
 */

export class ReaderView {
  /** @type {Map<import("golden-layout").ComponentContainer, EditorComponent>} */
  #map = new Map();
  #rect = new DOMRect();

  /** @type {HTMLElement} */
  #root;

  /** @type {import("golden-layout").GoldenLayout} */
  #layout;

  /**
   *
   * @param {HTMLElement} [root]
   */
  constructor(root) {
    this.#root = root || getDefaultParent();
    this.loadStyles();
    this.loadLayout();
  }

  loadStyles() {
    Object.assign(this.#root.style, {
      position: "relative",
      height: "100%",
      width: "100%",
      overflow: "clip",
    });

    document.head.append(
      createStyle({
        html: {
          height: "100%",
          width: "100%",
        },
        body: {
          height: "100%",
          width: "100%",
          margin: 0,
          overflow: "hidden",
        },
        ".cm-editor": {
          height: "100%",
        },
        ".cm-scroller": {
          overflow: "auto",
        },
      }),
      createStyle(glBase),
      createStyle(glLightTheme)
    );
  }

  loadLayout() {
    this.#layout = new GoldenLayout(this.#root, this.#bind, this.#unbind);
    this.#layout.resizeWithContainerAutomatically = true;

    this.#layout.beforeVirtualRectingEvent = (count) => {
      this.#rect = this.#root.getBoundingClientRect();
    };

    this.#layout.loadLayout({
      root: {
        type: "stack",
        content: [{ type: "component", componentType: "editor" }],
      },
    });
  }

  /**
   *
   * @param {import("golden-layout").ComponentContainer} container
   * @returns
   */
  #getComponent(container) {
    const component = this.#map.get(container);
    if (!component)
      throw new Error(`Missing component for container(${container.title})`);

    return component;
  }

  /** @type {import("golden-layout").VirtualLayout.BindComponentEventHandler} */
  #bind = (container, itemConfig) => {
    const { componentType } = itemConfig;
    /** @type {EditorComponent} */
    const component = this[`${componentType}Factory`](container, itemConfig);

    this.#map.set(container, component);
    this.#root.appendChild(component.rootHtmlElement);

    container.setTitle(component.path);
    container.virtualRectingRequiredEvent = this.#recting;
    container.virtualVisibilityChangeRequiredEvent = this.#visibilityChange;
    container.virtualZIndexChangeRequiredEvent = this.#zIndexChange;

    return { component, virtual: true };
  };

  /** @type {import("golden-layout").VirtualLayout.UnbindComponentEventHandler} */
  #unbind = (container) => {
    const component = this.#getComponent(container);
    this.#root.removeChild(component.rootHtmlElement);
    this.#map.delete(container);
  };

  /** @type {import("golden-layout").ComponentContainer.VirtualRectingRequiredEvent} */
  #recting = (container, width, height) => {
    const parent = this.#getComponent(container).rootHtmlElement;
    const rect = container.element.getBoundingClientRect();
    parent.style.left = `${rect.left - this.#rect.left}px`;
    parent.style.top = `${rect.top - this.#rect.top}px`;
    parent.style.width = `${width}px`;
    parent.style.height = `${height}px`;
  };

  /** @type {import("golden-layout").ComponentContainer.VirtualVisibilityChangeRequiredEvent} */
  #visibilityChange = (container, visible) => {
    const parent = this.#getComponent(container).rootHtmlElement;
    parent.style.display = visible ? "" : "none";
  };

  /** @type {import("golden-layout").ComponentContainer.VirtualZIndexChangeRequiredEvent} */
  #zIndexChange = (container, logicalZIndex, defaultZIndex) => {
    const parent = this.#getComponent(container).rootHtmlElement;
    parent.style.zIndex = defaultZIndex;
  };

  /**
   *
   * @param {import("golden-layout").ComponentContainer} container
   * @param {import("golden-layout").ResolvedComponentItemConfig} param1
   * @returns {EditorComponent}
   */
  editorFactory(container, { componentType, componentState }) {
    const parent = document.createElement("div");
    parent.style.position = "absolute";
    parent.style.overflow = "hidden";

    if (!componentState || typeof componentState != "object")
      throw new Error("Invalid component state");

    const id = getId(componentState);
    const node = getScript(id, "source");

    const path = node.dataset.path;
    if (!path) throw new Error(`Missing source path for id(${id})`);

    const editor = createEditor({ parent, id, lines: node.data });
    editor.dom.addEventListener("OpenFile", this.#onOpenFile(componentType));

    return { container, id, path, editor, rootHtmlElement: parent };
  }

  /**
   *
   * @param {import("golden-layout").ResolvedComponentItemConfig['componentType']} componentType
   * @returns {EventListener}
   */
  #onOpenFile(componentType) {
    return (ev) => {
      this.#layout.addComponent(
        componentType,
        /** @type {CustomEvent} */ (ev).detail
      );
    };
  }
}

class RangeValue extends BaseRangeValue {
  #eq;
  #value;

  get value() {
    return this.#value;
  }

  /**
   *
   * @param {any} value
   * @param {(a: any, b: any) => boolean} eq
   */
  constructor(value, eq = isEqual) {
    super();
    this.#value = value;
    this.#eq = eq;
  }

  /**
   *
   * @param {RangeValue} other
   * @returns
   */
  eq(other) {
    return this.#eq(this.#value, other.value);
  }
}

/**
 *
 * @param {Object | string} x
 * @returns
 */
function createStyle(x) {
  const node = document.createElement("style");
  if (typeof x == "string") {
    node.textContent = x;
  } else {
    const styles = [];
    for (const selector in x) {
      const style = x[selector];
      styles.push(`${selector} {${styleToCSS(style)}}`);
    }
    node.textContent = styles.join("\n");
  }
  return node;
}

/**
 *
 * @param {Element} element
 * @param {any} detail
 */
function dispatchOpenFileEvent(element, detail) {
  element.dispatchEvent(new CustomEvent("OpenFile", { detail }));
}

/**
 * @typedef EditorConfig
 * @type {{
 *   id: number,
 *   parent: Element | DocumentFragment,
 *   lines: string[],
 * }}
 */

/**
 *
 * @param {EditorConfig} config
 */
function createEditor({ id, parent, lines }) {
  return new EditorView({
    parent,
    doc: Text.of(lines),
    extensions: [
      EditorState.readOnly.of(true),
      lineNumbers(),
      highlightActiveLineGutter(),
      highlightActiveLine(),
      semantics(getData(id, "semantics")),
      link(getData(id, "link")),
      lint(getData(id, "lint")),
    ],
  });
}

/**
 *
 * @param {number} x
 */
function sign(x) {
  return x < 0 ? -1 : x > 0 ? 1 : 0;
}

/**
 * Retrieve the last value corresponding the given StateEffect.
 * @template T
 * @param {import("@codemirror/state").Transaction} tr
 * @param {import("@codemirror/state").StateEffectType<T>} valueEffect
 * @returns {T | undefined}
 */
export function getLastValueFromTransaction(tr, valueEffect) {
  let value;
  for (const effect of tr.effects) {
    if (effect.is(valueEffect)) {
      value = effect.value;
    }
  }
  return value;
}

/**
 *
 * @returns {number}
 */
function getDefaultId() {
  /** @type {HTMLScriptElement | null} */
  const node = document.querySelector("script[data-main]");
  if (!node) throw new Error("Missing main <script>");

  const id = node.dataset.id;
  if (id === undefined) throw new Error("Missing data-id");

  const value = Number(id);
  if (!Number.isFinite(value)) throw new Error("Invalid ID");

  return value;
}

function getDefaultParent() {
  const element = document.body.firstElementChild;
  return element instanceof HTMLDivElement && element.childElementCount === 0
    ? element
    : document.body;
}

/**
 *
 * @param {Object} state
 * @returns {number}
 */
function getId(state) {
  const id = state["id"];
  if (id != null) {
    if (typeof id !== "number") throw new Error(`Invalid id(${id})`);
    return id;
  }

  return getDefaultId();
}

/**
 *
 * @param {number} id
 * @param {ScriptType} type
 * @return
 */
function getScript(id, type) {
  /** @type {HTMLScriptElement & {data: any[]} | null} */
  const node = document.querySelector(
    `script[data-id="${id}"][data-type="${type}"]`
  );
  if (!node) throw new Error(`Unknown <script> for id(${id}) type(${type})`);
  return node;
}

/**
 *
 * @param {number} id
 * @param {ScriptType} type
 * @returns {any[]}
 */
function getData(id, type) {
  return getScript(id, type).data;
}

/**
 * Translate 1-based line/column to 0-based offset.
 * @param {import("@codemirror/state").Text} doc
 * @param {number} row
 * @param {number} col
 */
function getOffset(doc, row, col) {
  return doc.line(row).from + col - 1;
}

/**
 *
 * @param {import("@codemirror/state").Text} doc
 * @param {number} beginRow
 * @param {number} beginCol
 * @param {number} endRow
 * @param {number} endCol
 */
function getRange(doc, beginRow, beginCol, endRow, endCol) {
  const from = getOffset(doc, beginRow, beginCol);
  const to = getOffset(doc, endRow, endCol);
  return [from, to];
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

        const [from, to] = getRange(doc, beginRow, beginCol, endRow, endCol);

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
        ".RAW": {
          textDecorationLine: "underline",
          textDecorationStyle: "wavy",
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
function link(data) {
  return StateField.define({
    create({ doc }) {
      /** @type {RangeSetBuilder<RangeValue>} */
      const builder = new RangeSetBuilder();

      for (let j = 0, n = data.length; j < n; ) {
        const beginRow = data[j++];
        const beginCol = data[j++];
        const endRow = data[j++];
        const endCol = data[j++];
        const file = data[j++];

        const [from, to] = getRange(doc, beginRow, beginCol, endRow, endCol);

        builder.add(from, to, new RangeValue(file));
      }
      return builder.finish();
    },
    update(value, tr) {
      return value;
    },
    provide: (f) => [
      hoverTooltip((view, pos, side) => {
        /** @type {import("@codemirror/view").Tooltip | null} */
        let tooltip = null;
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
              const follow = dom.appendChild(document.createElement("span"));
              follow.className = "follow";
              follow.textContent = "Follow link";
              follow.addEventListener("click", () => {
                dispatchOpenFileEvent(view.dom, { id: value });
              });
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
          fontStyle: "italic",
          content: `"header "`,
        },
        ".link .file": {
          fontWeight: "bold",
          quotes: `'"' '"'`,
        },
        ".link .file::before": {
          content: "open-quote",
        },
        ".link .file::after": {
          content: "close-quote",
        },
        ".link > .follow": {
          margin: "5px",
          cursor: "pointer",
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
function lint(data) {
  return StateField.define({
    create({ doc }) {
      return [];
    },
    update(value, tr) {
      return value;
    },
    provide: (f) => [linter((view) => view.state.field(f))],
  });
}

/**
 *
 * @param {any[]} data
 * @returns
 */
// function decl(data) {
//   if (!validate(data, "decl")) return nul;

//   const first = ++i;
//   while (validate(data)) i += 8;
//   const last = i;

//   return StateField.define({
//     create({ doc }) {
//       const builder = new RangeSetBuilder();

//       for (let j = first; j < last; ) {
//         const tokBeginRow = data[j++];
//         const tokBeginCol = data[j++];
//         const name = data[j++];
//         const kind = data[j++];
//         const specs = data[j++];
//         const elaboratedType = data[j++];
//         const qualifiedType = data[j++];
//         const desugaredType = data[j++];

//         const from = doc.line(tokBeginRow).from + tokBeginCol - 1;
//         const to = from + name.length;

//         builder.add(
//           from,
//           to,
//           new RangeValue({
//             name,
//             kind,
//             specs,
//             elaboratedType,
//             qualifiedType,
//             desugaredType,
//           })
//         );
//       }
//       return builder.finish();
//     },
//     update(value, tr) {
//       return value;
//     },
//     provide: (f) => [
//       hoverTooltip((view, pos, side) => {
//         const specNames = {
//           1: "extern",
//           2: "static",
//           4: "inline",
//           8: "const",
//           16: "volatile",
//         };
//         const elaboratedNames = ["", "struct", "union", "enum"];

//         let tooltip;
//         view.state
//           .field(f)
//           .between(
//             pos,
//             pos,
//             (
//               from,
//               to,
//               {
//                 value: {
//                   name,
//                   kind,
//                   specs,
//                   elaboratedType,
//                   qualifiedType,
//                   desugaredType,
//                 },
//               }
//             ) => {
//               tooltip = {
//                 pos: from,
//                 end: to,
//                 create(view) {
//                   const dom = document.createElement("div");
//                   dom.className = "decl";

//                   const sp = dom.appendChild(document.createElement("span"));
//                   sp.className = "specifier";
//                   for (const k in specNames)
//                     if (specs & k) sp.append(specNames[k], " ");

//                   sp.append(elaboratedNames[elaboratedType]);

//                   const id = dom.appendChild(document.createElement("span"));
//                   id.className = `identifier ${kind}`;
//                   id.textContent = name;

//                   dom.append(": ");

//                   const t = dom.appendChild(document.createElement("span"));
//                   t.className = "type";
//                   t.textContent = qualifiedType;

//                   if (desugaredType && desugaredType !== qualifiedType) {
//                     const t = dom.appendChild(document.createElement("span"));
//                     t.className = "desugared type";
//                     t.textContent = desugaredType;
//                   }

//                   return { dom };
//                 },
//               };
//             }
//           );
//         return tooltip;
//       }),
//       EditorView.baseTheme({
//         ".decl": {
//           fontFamily: "monospace",
//           margin: "5px",
//         },

//         ".decl .specifier": {
//           color: "#0000ff",
//         },
//         ".decl .specifier::after": {
//           content: `" "`,
//         },

//         ".decl .identifier": {
//           fontWeight: "bold",
//         },
//         // Set common styles for func/var/type tag.
//         ".decl .identifier::before": {
//           color: "#808080",
//           fontStyle: "italic",
//           fontWeight: "normal",
//         },
//         ".FunctionDecl::before": {
//           content: `"func "`,
//         },
//         ".FunctionDecl": {
//           color: "#795E26",
//         },

//         ".VarDecl::before": {
//           content: `"var "`,
//         },
//         ".VarDecl": {
//           color: "#001080",
//         },

//         ".ParmVarDecl::before": {
//           content: `"param "`,
//         },
//         ".ParmVarDecl": {
//           color: "#808080",
//         },

//         ".FieldDecl::before": {
//           content: `"field "`,
//         },
//         ".FieldDecl": {
//           color: "#0451a5",
//         },

//         ".TypedefDecl::before": {
//           content: `"type "`,
//         },
//         ".TypedefDecl": {
//           color: "#267f99",
//         },

//         ".decl .type": {
//           color: "#267f99",
//         },
//         ".decl .desugared.type::before": {
//           content: `":"`,
//         },
//         ".decl .desugared.type": {
//           backgroundColor: "#E5EBF1",
//           borderRadius: "4px",
//         },
//       }),
//     ],
//   });
// }

/**
 *
 * @param {any[]} data
 * @returns
 */
// function macroDecl(data) {
//   if (!validate(data, "macro_decl")) return nul;

//   const first = ++i;
//   while (validate(data)) i += 5;
//   const last = i;

//   return StateField.define({
//     create({ doc }) {
//       const builder = new RangeSetBuilder();

//       for (let j = first; j < last; ) {
//         const tokBeginRow = data[j++];
//         const tokBeginCol = data[j++];
//         const name = data[j++];
//         const parameters = data[j++];
//         const body = data[j++];

//         const from = doc.line(tokBeginRow).from + tokBeginCol - 1;
//         const to = from + name.length;

//         builder.add(from, to, new RangeValue({ name, parameters, body }));
//       }
//       return builder.finish();
//     },
//     update(value, tr) {
//       return value;
//     },
//     provide: (f) => [
//       hoverTooltip((view, pos, side) => {
//         let tooltip;
//         view.state
//           .field(f)
//           .between(
//             pos,
//             pos,
//             (from, to, { value: { name, parameters, body } }) => {
//               tooltip = {
//                 pos: from,
//                 end: to,
//                 create(view) {
//                   const dom = document.createElement("div");
//                   dom.className = "macro_decl";
//                   const id = dom.appendChild(document.createElement("span"));
//                   id.className = "identifier";
//                   id.textContent = name;
//                   if (parameters) dom.append(parameters);
//                   if (body) {
//                     dom.append(" ");
//                     const code = dom.appendChild(
//                       document.createElement("code")
//                     );
//                     code.textContent = body;
//                   }
//                   return { dom };
//                 },
//               };
//             }
//           );
//         return tooltip;
//       }),
//       EditorView.baseTheme({
//         ".macro_decl": {
//           fontFamily: "monospace",
//           margin: "5px",
//         },

//         ".macro_decl::before": {
//           color: "#808080",
//           fontStyle: "italic",
//           content: `"macro "`,
//         },

//         ".macro_decl .identifier": {
//           fontWeight: "bold",
//           color: "#0000ff",
//         },

//         ".macro_decl code": {
//           whiteSpace: "pre",
//         },
//       }),
//     ],
//   });
// }

// const testMacroDecl = macroDecl;
