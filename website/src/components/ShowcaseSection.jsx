export function ShowcaseSection({ sampleCode }) {
  return (
    <section className="showcase">
      <article className="code-panel panel">
        <div className="code-panel-bar">
          <span />
          <span />
          <span />
        </div>
        <pre>
          <code>{sampleCode}</code>
        </pre>
      </article>

      <article className="story-panel panel">
        <p className="eyebrow">Syntax at a glance</p>
        <h2>Readable declarations, explicit semantics, and clear module boundaries.</h2>
        <p>
          The draft guide emphasizes conformance, predictable module resolution,
          secure handling of optional and throwable values, and a language
          surface that scales from scripts to compiled modules.
        </p>
        <ul className="story-list">
          <li>Classes, interfaces, structs, and enums live alongside scopes and aliases.</li>
          <li>Arrays, maps, tasks, function types, casts, and runtime type checks are first-class.</li>
          <li>UTF-8 source, actionable diagnostics, and deterministic outputs are core expectations.</li>
        </ul>
      </article>
    </section>
  );
}
