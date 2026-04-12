export function ToolchainSection({ commands }) {
  return (
    <section id="toolchain" className="section">
      <div className="section-heading">
        <p className="eyebrow">Toolchain</p>
        <h2>Everything you need from local execution to editor intelligence.</h2>
      </div>

      <div className="command-grid">
        {commands.map((item) => (
          <article key={item.command} className="command-card panel">
            <div className="command-pill">{item.command}</div>
            <h3>{item.title}</h3>
            <p>{item.body}</p>
          </article>
        ))}
      </div>
    </section>
  );
}
