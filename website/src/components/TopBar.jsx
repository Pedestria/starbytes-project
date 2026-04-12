import logo from "../../../public/STD Logo V4.png";

export function TopBar() {
  return (
    <header className="topbar">
      <a className="brand" href="#top" aria-label="Starbytes home">
        <img className="brand-logo" src={logo} alt="Starbytes logo" />
        <span>Starbytes</span>
      </a>

      <nav className="topnav" aria-label="Primary">
        <a href="#language">Language</a>
        <a href="#toolchain">Toolchain</a>
        <a href="#spec">Specification</a>
      </nav>
    </header>
  );
}
