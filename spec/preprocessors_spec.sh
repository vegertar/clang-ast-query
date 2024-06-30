Context
  setup() {
    db=$(mktemp /tmp/clang-ast-query-db.XXXXXX)
    ./caq -o $db samples/preprocessors.c
  }
  cleanup() { rm $db; }
  BeforeAll 'setup'
  AfterAll 'cleanup'

  Describe 'Macro expansion in #if'
    Parameters

    End

    It "compares the decl $1"
      When call query "$2" "$3" "$4" "$5" "$6"
      The output should eq 0
    End
  End
End