"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const YAML = require("js-yaml");
const fs = require("fs/promises");
const path = require("path");
const prettier_1 = require("prettier");
function isTextMateRule(arg) {
    if (arg.include) {
        return false;
    }
    else {
        return true;
    }
}
/**
 * Transforms Sublime TextMate Syntax Variables in tmLanguage.yml files.
 * Outputs to a `.json` with same file name
 * @param {string} file
 */
async function build(file) {
    let TMGrammar = YAML.safeLoad((await fs.readFile(file, "utf-8")));
    let outputFile = path.dirname(file) + '/' + path.basename(file, "yml") + "json";
    //Get Variables
    let VariablesMatchers = [];
    for (let varname in TMGrammar.variables) {
        for (let { regex, replacement } of VariablesMatchers) {
            TMGrammar.variables[varname] = TMGrammar.variables[varname].replace(regex, replacement);
        }
        VariablesMatchers.push({ regex: new RegExp(`\{\{${varname}\}\}`, "gm"), replacement: TMGrammar.variables[varname] });
    }
    function replaceVariables(subject) {
        let result;
        for (let { regex, replacement } of VariablesMatchers) {
            result = subject.replace(regex, replacement);
            subject = result;
        }
        return result;
    }
    function recursiveBuild(rule) {
        if (rule.begin) {
            rule.begin = replaceVariables(rule.begin);
        }
        if (rule.end) {
            rule.end = replaceVariables(rule.end);
        }
        if (rule.match) {
            rule.match = replaceVariables(rule.match);
        }
        if (rule.patterns) {
            for (let ruleORInclude of rule.patterns) {
                if (isTextMateRule(ruleORInclude)) {
                    recursiveBuild(ruleORInclude);
                }
            }
        }
    }
    for (let ruleORInclude of TMGrammar.patterns) {
        if (isTextMateRule(ruleORInclude)) {
            recursiveBuild(ruleORInclude);
        }
    }
    for (let key in TMGrammar.repository) {
        if (isTextMateRule(TMGrammar.repository[key])) {
            recursiveBuild(TMGrammar.repository[key]);
        }
    }
    let result = {
        name: TMGrammar.name,
        scopeName: TMGrammar.scopeName,
        foldingStartMarker: TMGrammar.foldingStartMarker,
        foldingStopMarker: TMGrammar.foldingStopMarker,
        fileTypes: TMGrammar.fileTypes,
        patterns: TMGrammar.patterns,
        repository: TMGrammar.repository
    };
    let options = {
        trailingComma: "all",
        parser: "json"
    };
    await fs.writeFile(outputFile, prettier_1.format(JSON.stringify(result), options));
}
build("./syntaxes/starbytes.tmLanguage.yml").catch(err => console.error(err));
