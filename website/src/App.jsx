import {
  commands,
  guidePoints,
  highlightCards,
  metrics,
  sampleCode
} from "./content";
import { FeatureSection } from "./components/FeatureSection";
import { HeroSection } from "./components/HeroSection";
import { ShowcaseSection } from "./components/ShowcaseSection";
import { SpecSection } from "./components/SpecSection";
import { ToolchainSection } from "./components/ToolchainSection";
import { TopBar } from "./components/TopBar";

export default function App() {
  return (
    <div className="site-shell">
      <div className="aurora aurora-left" aria-hidden="true" />
      <div className="aurora aurora-right" aria-hidden="true" />

      <TopBar />

      <main id="top">
        <HeroSection metrics={metrics} />
        <FeatureSection cards={highlightCards} />
        <ShowcaseSection sampleCode={sampleCode} />
        <ToolchainSection commands={commands} />
        <SpecSection guidePoints={guidePoints} />
      </main>
    </div>
  );
}
