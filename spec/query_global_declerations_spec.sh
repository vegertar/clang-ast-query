create() {
  db=$(mktemp /tmp/clang-ast-query-db.XXXXXX)
  ./test.out -x $db -l 2 $1
}

Describe 'Query Global Declerations'
  setup() { :; }
  cleanup() { rm $db; }
  BeforeEach 'setup'
  AfterEach 'cleanup'

  Describe 'main()'
    It 'query the function name'
      query_name() {
        echo SELECT 'name' FROM ast \
          WHERE 'line =' $1 AND 'col <=' $2 \
          AND 'col + length(name) >' $2
      }

      query() {
        sqlite3 $db "`query_name 1 7`"
      }

      create samples/main.ast
      When call query
      The output should equal main
    End

    It 'query the parameter names'
      query_id() {
        echo SELECT 'id' FROM ast \
          WHERE 'line =' $1 AND 'col <=' $2 \
          AND 'col + length(name) >' $2
      }

      query_params() {
        echo SELECT 'name' FROM ast WHERE id in '(' \
          SELECT 'id' FROM hierarchy WHERE 'parent' = \'$1\' ')'
      }

      query() {
        id=$(sqlite3 $db "`query_id 1 7`")
        sqlite3 $db "`query_params ${id}`"
      }

      create samples/main.ast
      When call query
      The line 1 of output should eq 'argc'
      The line 2 of output should eq 'argv'
    End
  End
End