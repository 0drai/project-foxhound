function regexMatchTest() {
    var a = 'Hello ';
    var b = taint('tainted');
    var c = ' World!';
    var str = a + b + c;

    // Test basic string matching
    var match = str.match('tainted')[0];
    assertEqualTaint(b, match);

    // Test basic regex matching
    match = str.match(/t.*ed/)[0];
    assertEqualTaint(b, match);

    // Test capture groups
    match = str.match(/ (\w+) /);
    assertEqualTaint(b, match[0].substring(1, match[0].length -1));
    assertEqualTaint(b, match[1]);

    // Test global matches
    match = str.match(/\w+/g);
    assertEqualTaint(b, match[1]);
}

runTaintTest(regexMatchTest);

if (typeof reportCompare === 'function')
  reportCompare(true, true);

