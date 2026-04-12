export function SpecSection({ guidePoints }) {
  return (
    <section id="spec" className="section split-section">
      <article className="panel spec-panel">
        <p className="eyebrow">Specification</p>
        <h2>Grounded in a draft ISO-style user guide.</h2>
        <p>
          The Starbytes user guide is written like a standards reference,
          spelling out lexical structure, type rules, declarations,
          expressions, control flow, lazy task semantics, module resolution,
          and output artifacts.
        </p>
      </article>

      <article className="panel points-panel">
        <ul className="points-list">
          {guidePoints.map((item) => (
            <li key={item}>{item}</li>
          ))}
        </ul>
      </article>
    </section>
  );
}
