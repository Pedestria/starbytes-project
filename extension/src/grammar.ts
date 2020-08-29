import * as YAML from 'js-yaml'
import * as fs from 'fs/promises'
import * as path from 'path'
import {format, Options} from 'prettier'

type TextMateMap <T> = {
    [s:string]:T
}

type TextMateInclude = {
    include: string
}

type TextMateRule = {
    match?:string
    begin?:string
    end?:string
    patterns?:TextMateRule|TextMateInclude[]
}

type TextMateGrammar = {
    name:string
    scopeName:string
    fileTypes:string[]
    foldingStartMarker:string
    foldingStopMarker:string
    patterns:TextMateRule[]
    repository:TextMateMap<TextMateRule>
    variables?:TextMateMap<string>
}

type RegexTextMateVariableMatcher = {
    regex:RegExp
    replacement:string
}

function isTextMateRule(arg:any): arg is TextMateRule{
    if(arg.include){
        return false
    } else {
        return true
    }
}
/**
 * Transforms Sublime TextMate Syntax Variables in tmLanguage.yml files.
 * Outputs to a `.json` with same file name
 * @param {string} file 
 */

async function build (file:string){
    let TMGrammar = YAML.safeLoad((await fs.readFile(file,"utf-8"))) as TextMateGrammar;
    let outputFile:string = path.dirname(file)+'/'+path.basename(file,"yml")+"json"
    //Get Variables
    let VariablesMatchers:RegexTextMateVariableMatcher[] = [];
    for(let varname in TMGrammar.variables){
        for(let {regex,replacement} of VariablesMatchers){
            TMGrammar.variables[varname] = TMGrammar.variables[varname].replace(regex,replacement);
        }
        VariablesMatchers.push({regex:new RegExp(`\{\{${varname}\}\}`,"gm"),replacement:TMGrammar.variables[varname]})
    }


    function replaceVariables(subject:string):string{
        let result:string
        for(let {regex,replacement} of VariablesMatchers){
            result = subject.replace(regex,replacement);
            subject = result;
        }
        return result;
    }

    
    function recursiveBuild(rule:TextMateRule){
        if(rule.begin){
            rule.begin = replaceVariables(rule.begin)
        }
        if(rule.end) {
            rule.end = replaceVariables(rule.end)
        }
        if(rule.match){
            rule.match = replaceVariables(rule.match)
        }
        if(rule.patterns){
            for(let key in rule.patterns){
                if(isTextMateRule(rule.patterns[key])){
                    recursiveBuild(rule.patterns[key])
                }
            }
        }
    }

    for (let key in TMGrammar.repository){
        if(isTextMateRule(TMGrammar.repository[key])){
            recursiveBuild(TMGrammar.repository[key]);
        }
    }

    let result:TextMateGrammar = {
        name:TMGrammar.name,
        scopeName:TMGrammar.scopeName,
        foldingStartMarker:TMGrammar.foldingStartMarker,
        foldingStopMarker:TMGrammar.foldingStopMarker,
        fileTypes:TMGrammar.fileTypes,
        patterns:TMGrammar.patterns,
        repository:TMGrammar.repository
    };
    let options:Options = {
        trailingComma:"all",
        parser:"json"
    };
    await fs.writeFile(outputFile,format(JSON.stringify(result),options));
}

build("./syntaxes/starbytes.tmLanguage.yml").catch(err => console.error(err));