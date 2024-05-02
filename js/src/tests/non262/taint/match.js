function matchTest() {
    var a = 'Hello ';
    var b = taint('tainted');
    var c = ' World!';
    var str = a + b + c;

    // For a flat match, the result is only required to be
    // tainted if the pattern string itself was tainted.
    var match = str.match(b)[0];
    assertEqualTaint(b, match);
    assertNotHasTaintOperation(str, 'match');

    // Test basic regex matching
    match = str.match(/t.*ed/)[0];
    assertEqualTaint(b, match);
    assertNotHasTaintOperation(str, 'match');

    // Test capture groups
    match = str.match(/ (\w+) /);
    assertEqualTaint(b, match[0].substring(1, match[0].length -1));
    assertEqualTaint(b, match[1]);
    assertNotHasTaintOperation(str, 'match');

    // Test global matches
    match = str.match(/\w+/g);
    assertEqualTaint(b, match[1]);
    assertNotHasTaintOperation(str, 'match');
}

runTaintTest(matchTest);

if (typeof reportCompare === 'function')
  reportCompare(true, true);

