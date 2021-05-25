import * as YAML from 'js-yaml'
import * as fs from 'fs/promises'
import * as path from 'path'
import {format, Options} from 'prettier'
import * as PLIST from 'plist'

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
    patterns?:(TextMateRule|TextMateInclude)[]
}

type TextMateGrammar = {
    name:string
    scopeName:string
    uuid:string
    fileTypes:string[]
    foldingStartMarker:string
    foldingStopMarker:string
    patterns:(TextMateRule|TextMateInclude)[]
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
 * Outputs to a `.json` with same file name. However if `plist` is set to `true`
 * Then it will output to a TM Plist file.
 * @param {string} file 
 * @param {boolean} plist
 * @returns {Promise<void>} A void
 */

async function build (file:string,plist:boolean = false):Promise<void>{
    let TMGrammar = YAML.safeLoad((await fs.readFile(file,"utf-8"))) as TextMateGrammar;
    let outputFile:string
    if(plist){
        outputFile = path.dirname(file)+'/'+path.basename(file,".yml");
    }
    else{
        outputFile = path.dirname(file)+'/'+path.basename(file,"yml")+"json";
    }
    
    
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
            for(let ruleORInclude of rule.patterns){
                if(isTextMateRule(ruleORInclude)){
                    recursiveBuild(ruleORInclude)
                }
            }
        }
    }

    for(let ruleORInclude of TMGrammar.patterns){
        if(isTextMateRule(ruleORInclude)){
            recursiveBuild(ruleORInclude);
        }
    }

    for (let key in TMGrammar.repository){
        if(isTextMateRule(TMGrammar.repository[key])){
            recursiveBuild(TMGrammar.repository[key]);
        }
    }

    let result:TextMateGrammar = {
        name:TMGrammar.name,
        uuid:TMGrammar.uuid,
        scopeName:TMGrammar.scopeName,
        fileTypes:TMGrammar.fileTypes,
        foldingStartMarker:TMGrammar.foldingStartMarker,
        foldingStopMarker:TMGrammar.foldingStopMarker,
        patterns:TMGrammar.patterns,
        repository:TMGrammar.repository
    };
    let options:Options = {
        trailingComma:"all",
        parser:"json"
    };

    if(plist){
        await fs.writeFile(outputFile,PLIST.build(JSON.parse(JSON.stringify(result)),{pretty:true}));
    }
    else{
        await fs.writeFile(outputFile,format(JSON.stringify(result),options));
    }
    
}

build("./syntaxes/starbytes.tmLanguage.yml",false).catch(err => console.error(err));

build("./syntaxes/starbytes-project.tmLanguage.yml",false).catch(err => console.log(err));