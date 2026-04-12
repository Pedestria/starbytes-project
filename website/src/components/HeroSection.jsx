import logo from "../../../public/STD Logo V4.png";

export function HeroSection({ metrics }) {
  return (
    <section className="hero panel">
      <div className="hero-copy">
        <p className="eyebrow">Strongly typed language + bytecode toolchain</p>
        <h1>Build software with a language spec, runtime, and editor tooling that move together.</h1>
        <p className="hero-text">
          Starbytes combines a modern typed language, native standard library,
          bytecode compiler/runtime, and compiler-backed developer tooling into
          one coherent platform.
        </p>

        <div className="hero-actions">
          <a className="button button-primary" href="#toolchain">
            Explore the toolchain
          </a>
          <a className="button button-secondary" href="#spec">
            Read the specification highlights
          </a>
        </div>

        <ul className="metric-grid" aria-label="Starbytes highlights">
          {metrics.map((item) => (
            <li key={item.label} className="metric-card">
              <strong>{item.value}</strong>
              <span>{item.label}</span>
            </li>
          ))}
        </ul>
      </div>

      <div className="hero-visual">
        <div className="logo-stage">
          <img src={logo} alt="" aria-hidden="true" />
        </div>
        <div className="hero-note">
          <span className="note-kicker">Draft spec direction</span>
          <p>
            The current Starbytes guide defines syntax, runtime semantics,
            module artifacts, diagnostics, and toolchain behavior in a
            standards-style structure.
          </p>
        </div>
      </div>
    </section>
  );
}
