Describe 'Query Global Declerations'
  Describe 'main'
    It 'query the main decleration'
      When call ./test.out samples/main.ast
      The status should be success
    End
  End
End