export function FeatureSection({ cards }) {
  return (
    <section id="language" className="section">
      <div className="section-heading">
        <p className="eyebrow">Language surface</p>
        <h2>Designed to be expressive, typed, and operationally clear.</h2>
      </div>

      <div className="card-grid">
        {cards.map((item) => (
          <article key={item.title} className="feature-card panel">
            <p className="eyebrow">{item.eyebrow}</p>
            <h3>{item.title}</h3>
            <p>{item.copy}</p>
          </article>
        ))}
      </div>
    </section>
  );
}
