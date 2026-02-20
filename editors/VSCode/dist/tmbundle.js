"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const fs = require("fs/promises");
async function main() {
    const bundleName = "./Starbytes.tmbundle";
    const syntaxesDir = `${bundleName}/Syntaxes`;
    try {
        await fs.rmdir(bundleName, { recursive: true });
    }
    catch (error) {
        if (error && error.code !== "ENOENT") {
            throw error;
        }
    }
    await fs.mkdir(syntaxesDir, { recursive: true });
    await Promise.all([
        fs.copyFile("./Info.plist", `${bundleName}/Info.plist`),
        fs.copyFile("./syntaxes/starbytes.tmLanguage", `${syntaxesDir}/Starbytes.tmLanguage`),
    ]);
}
main().catch((error) => {
    console.error(error);
    process.exitCode = 1;
});
