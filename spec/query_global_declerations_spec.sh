create() {
  db=$(mktemp /tmp/clang-ast-query-db.XXXXXX)
  ./test.out -x $1 | sqlite3 $db
}

query() {
  echo SELECT 'decl$name' FROM ast \
    WHERE 'loc$line =' $1 AND 'loc$col <=' $2 \
    AND 'loc$col + length(decl$name) >' $2
}

Describe 'Query Global Declerations'
  setup() { :; }
  cleanup() { rm $db; }
  BeforeRun 'setup'
  AfterRun 'cleanup'

  Describe 'main'
    It 'query the main decleration'
      create samples/main.ast
      When call sqlite3 $db "`query 1 7`"
      The output should equal main
    End
  End
End