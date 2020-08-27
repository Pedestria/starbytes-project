#pragma once

namespace Starbytes {
    /*A Position on A Starbytes Document. Used to determine Positions of Tokens/ASTNodes/etc.
    */
    struct DocumentPosition {
        int line;
        int start;
        int end;
    };
}