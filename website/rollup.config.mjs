import babel from "@rollup/plugin-babel";
import commonjs from "@rollup/plugin-commonjs";
import nodeResolve from "@rollup/plugin-node-resolve";
import replace from "@rollup/plugin-replace";
import terser from "@rollup/plugin-terser";
import url from "@rollup/plugin-url";
import postcss from "rollup-plugin-postcss";

const production = process.env.NODE_ENV === "production";

export default {
  input: "src/main.jsx",
  output: {
    file: "dist/bundle.js",
    format: "esm",
    sourcemap: true
  },
  plugins: [
    replace({
      preventAssignment: true,
      "process.env.NODE_ENV": JSON.stringify(
        production ? "production" : "development"
      )
    }),
    nodeResolve({
      extensions: [".js", ".jsx"]
    }),
    commonjs(),
    url({
      include: ["**/*.png", "**/*.jpg", "**/*.jpeg", "**/*.gif", "**/*.svg"],
      limit: 0,
      fileName: "assets/[name]-[hash][extname]"
    }),
    postcss({
      extract: "bundle.css",
      minimize: production
    }),
    babel({
      babelHelpers: "bundled",
      extensions: [".js", ".jsx"],
      exclude: "node_modules/**",
      presets: [
        [
          "@babel/preset-env",
          {
            targets: "defaults"
          }
        ],
        [
          "@babel/preset-react",
          {
            runtime: "automatic"
          }
        ]
      ]
    }),
    production && terser()
  ]
};
