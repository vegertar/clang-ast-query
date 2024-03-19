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

  Describe 'Consistent decl in macro expansion'
    Parameters
      N 35 1 0 # the macro expansion point
      foo 28 2 1 19 1 # the 1st token after the expansion point
      bar 29 2 3 15 1 # the 3ed
      n 30 2 5 2 1    # the 5th
      y 31 2 9 6 1    # the 9th
      x 32 2 11 5 1   # the 11th
      n 33 2 16 10 1  # the 16th
    End

    query() {
      a=$(sqlite3 $db "`query_decl_number $1 $2 $3`")
      if [ $3 -eq 0 ]; then
        b=-1
      else
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
