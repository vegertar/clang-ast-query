import resolve from "rollup-plugin-node-resolve";
import commonjs from "rollup-plugin-commonjs";
import postcss from "rollup-plugin-postcss";
import url from "postcss-url";

export default {
  input: "reader.js",
  plugins: [
    resolve(),
    commonjs(),
    postcss({
      inject: false,
      plugins: [
        url({
          url: "inline", // enable inline assets using base64 encoding
          maxSize: 10, // maximum file size to inline (in kilobytes)
          fallback: "copy", // fallback method to use if max size is exceeded
        }),
      ],
    }),
  ],
  output: {
    file: "reader.bundle.js",
    format: "es",
  },
};
