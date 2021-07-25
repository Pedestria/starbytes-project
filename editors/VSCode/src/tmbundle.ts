import * as fs from "fs/promises"

let bundle_name = "./Starbytes.tmbundle"

let d = fs.mkdir(bundle_name)
d.then(() => {
    let syn = fs.mkdir(`${bundle_name}/Syntaxes`)
    syn.then(() => {
        fs.copyFile("./Info.plist",`${bundle_name}/Info.plist`)
        fs.copyFile("./syntaxes/starbytes.tmlanguage",`${bundle_name}/Syntaxes/Starbytes.tmlanguage`)
    })

})
