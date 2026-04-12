export const highlightCards = [
  {
    eyebrow: "Language",
    title: "Strong typing without giving up velocity",
    copy:
      "Classes, interfaces, structs, enums, generics, function types, lazy tasks, and secure error handling are all part of the core language surface."
  },
  {
    eyebrow: "Toolchain",
    title: "One project, multiple production tools",
    copy:
      "Starbytes ships with run, compile, and check commands, a bytecode runtime, a disassembler, a language server, and a shared linguistics engine."
  },
  {
    eyebrow: "Standard Library",
    title: "Native modules where real work happens",
    copy:
      "Built-in modules cover IO, FS, JSON, HTTP, Net, Text, Unicode, Crypto, Compression, Archive, Math, Random, and more."
  }
];

export const metrics = [
  { value: "0.12.0", label: "Current draft version" },
  { value: "3", label: "Core toolchain commands" },
  { value: "19", label: "Native stdlib modules" },
  { value: "LSP", label: "Compiler-backed editor support" }
];

export const commands = [
  {
    command: "run",
    title: "Execute scripts and modules",
    body:
      "Use the main driver to run a single file or a directory module with the same semantics the compiler understands."
  },
  {
    command: "compile",
    title: "Produce bytecode artifacts",
    body:
      "Compile source into .starbmod outputs, generate interface artifacts, and keep cache behavior deterministic."
  },
  {
    command: "check",
    title: "Validate before release",
    body:
      "Run parser and semantic validation early so failures are actionable, loud, and easy to diagnose."
  }
];

export const guidePoints = [
  "ISO-style draft language specification with explicit conformance requirements.",
  "Deterministic module resolution with support for .starbmodpath search roots.",
  "Actionable diagnostics across parser, semantic, and runtime failures.",
  "Shared linguistics layer for formatting, linting, suggestions, and code actions."
];

export const sampleCode = `import IO
import JSON

scope Build {
    func banner(version:String, channel:String) String {
        return "Starbytes " + version + " (" + channel + ")"
    }
}

decl raw = IO.readText("./release.json", "utf-8")
decl payload = JSON.parse(raw)

decl version = String(payload["version"])
decl channel = String(payload["channel"])
decl label = Build.banner(version, channel)
`;
