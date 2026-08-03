" DogScript syntax file
if exists("b:current_syntax")
  finish
endif

syn keyword dogKeyword func create exec var class extends if elif else then while for in do return use new try catch finally lambda cast const null true false
syn keyword dogType str int float bool list map

syn match dogNumber "\v<\d+\.\d+>"
syn match dogNumber "\v<\d+>"
syn region dogString start=/"/ skip=/\\"/ end=/"/
syn match dogOperator "=>\|!=\|=\|+\|-\|\*\|/\|<\|>"
syn match dogComment "--.*$"
syn region dogBlockComment start="-\[" end="\]-"

syn match dogFunctionDecl "\vfunc\s+create\s+\zs\w+"
syn match dogFunctionExec "\vfunc\s+exec\s+\zs\w+"
syn match dogClassDecl "\vclass\s+create\s+\zs\w+"
syn match dogImport "\vuse\s+<\zs\w+\ze>"

hi def link dogKeyword Keyword
hi def link dogType Type
hi def link dogNumber Number
hi def link dogString String
hi def link dogOperator Operator
hi def link dogComment Comment
hi def link dogBlockComment Comment
hi def link dogFunctionDecl Function
hi def link dogFunctionExec Function
hi def link dogClassDecl Type
hi def link dogImport Include

let b:current_syntax = "dogscript"
