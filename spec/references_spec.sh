Context
  setup() {
    db=$(mktemp /tmp/clang-ast-query-db.XXXXXX)
    ./caq -o $db samples/references.c
  }
  cleanup() { rm $db; }
  BeforeAll 'setup'
  AfterAll 'cleanup'

  query_decl_number() {
    echo SELECT decl FROM tok WHERE begin_row = $1 AND begin_col = $2
  }

  Describe 'Consistent decl'
    Parameters
      n 3 1 2 1 # reference parameters
    End

    query() {
      a=$(sqlite3 $db "`query_decl_number $1 $2`")
      if [ $3 -eq 0 ]; then
        b=-1
      else
        b=$(sqlite3 $db "`query_decl_number $3 $4`")
      fi
      echo $((a-b))
    }

    It "compares the decl $1"
      When call query "$2" "$3" "$4" "$5"
      The output should eq 0
    End
  End
End
