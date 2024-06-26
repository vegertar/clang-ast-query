Context
  setup() {
    db=$(mktemp /tmp/clang-ast-query-db.XXXXXX)
    ./caq -o $db samples/references.c
  }
  cleanup() { rm $db; }
  BeforeAll 'setup'
  AfterAll 'cleanup'

  query_decl_number() {
    echo SELECT decl FROM tok WHERE begin_row = $1 AND begin_col = $2 AND offset = ${3:-0}
  }

  query_decl_number_from_ast() {
    echo SELECT number FROM ast WHERE begin_row = $1 AND begin_col = $2
  }

  query_decl_number_from_ref() {
    echo SELECT number from ast WHERE ptr = '('SELECT ref_ptr FROM ast WHERE begin_row = $1 AND begin_col = $2')'
  }

  Describe 'Consistent decl'
    Parameters
      n 3 1 2 1 # reference parameters
      n 24 1 2 1 # array subscripts
      n 25 1 10 1 # struct fields
      foo 22 1 19 1 # local variables
      bar 23 1 15 1 # struct fields
    End

    query() {
      a=$(sqlite3 $db "`query_decl_number $1 $2`")
      b=$(sqlite3 $db "`query_decl_number $3 $4`")
      echo $((a-b))
    }

    It "compares the decl $1"
      When call query "$2" "$3" "$4" "$5"
      The output should eq 0
    End
  End

  Describe 'Consistent decl in macro expansion'
    Parameters
      N 36 1 0 28 2   # the macro expansion point
      foo 29 2 1 19 1 # the 1st token after the expansion point
      bar 30 2 3 15 1 # the 3ed
      n 31 2 5 2 1    # the 5th
      y 32 2 9 6 1    # the 9th
      x 33 2 11 5 1   # the 11th
      n 34 2 16 10 1  # the 16th
    End

    query() {
      if [ $3 -eq 0 ]; then
        a=$(sqlite3 $db "`query_decl_number_from_ref $1 $2`")
        b=$(sqlite3 $db "`query_decl_number_from_ast $4 $5`")
      else
        a=$(sqlite3 $db "`query_decl_number $1 $2 $3`")
        b=$(sqlite3 $db "`query_decl_number $4 $5`")
      fi

      echo $((a-b))
    }

    It "compares the decl $1"
      When call query "$2" "$3" "$4" "$5" "$6"
      The output should eq 0
    End
  End
End
