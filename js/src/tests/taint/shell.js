/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Token to indicate the end of a string.
var STR_END = -1;

if (typeof assertDeepEq === 'undefined') {
    var assertDeepEq = (function(){
        var call = Function.prototype.call,
            Array_isArray = Array.isArray,
            Map_ = Map,
            Error_ = Error,
            Symbol_ = Symbol,
            Map_has = call.bind(Map.prototype.has),
            Map_get = call.bind(Map.prototype.get),
            Map_set = call.bind(Map.prototype.set),
            Object_toString = call.bind(Object.prototype.toString),
            Function_toString = call.bind(Function.prototype.toString),
            Object_getPrototypeOf = Object.getPrototypeOf,
            Object_hasOwnProperty = call.bind(Object.prototype.hasOwnProperty),
            Object_getOwnPropertyDescriptor = Object.getOwnPropertyDescriptor,
            Object_isExtensible = Object.isExtensible,
            Object_getOwnPropertyNames = Object.getOwnPropertyNames,
            uneval_ = uneval;

        // Return true iff ES6 Type(v) isn't Object.
        // Note that `typeof document.all === "undefined"`.
        function isPrimitive(v) {
            return (v === null ||
                    v === undefined ||
                    typeof v === "boolean" ||
                    typeof v === "number" ||
                    typeof v === "string" ||
                    typeof v === "symbol");
        }

        function assertSameValue(a, b, msg) {
            try {
                assertEq(a, b);
            } catch (exc) {
                throw Error_(exc.message + (msg ? " " + msg : ""));
            }
        }

        function assertSameClass(a, b, msg) {
            var ac = Object_toString(a), bc = Object_toString(b);
            assertSameValue(ac, bc, msg);
            switch (ac) {
            case "[object Function]":
                assertSameValue(Function_toString(a), Function_toString(b), msg);
            }
        }

        function at(prevmsg, segment) {
            return prevmsg ? prevmsg + segment : "at _" + segment;
        }

        // Assert that the arguments a and b are thoroughly structurally equivalent.
        //
        // For the sake of speed, we cut a corner:
        //        var x = {}, y = {}, ax = [x];
        //        assertDeepEq([ax, x], [ax, y]);  // passes (?!)
        //
        // Technically this should fail, since the two object graphs are different.
        // (The graph of [ax, y] contains one more object than the graph of [ax, x].)
        //
        // To get technically correct behavior, pass {strictEquivalence: true}.
        // This is slower because we have to walk the entire graph, and Object.prototype
        // is big.
        //
        return function assertDeepEq(a, b, options) {
            var strictEquivalence = options ? options.strictEquivalence : false;

            function assertSameProto(a, b, msg) {
                check(Object_getPrototypeOf(a), Object_getPrototypeOf(b), at(msg, ".__proto__"));
            }

            function failPropList(na, nb, msg) {
                throw Error_("got own properties " + uneval_(na) + ", expected " + uneval_(nb) +
                             (msg ? " " + msg : ""));
            }

            function assertSameProps(a, b, msg) {
                var na = Object_getOwnPropertyNames(a),
                    nb = Object_getOwnPropertyNames(b);
                if (na.length !== nb.length)
                    failPropList(na, nb, msg);

                // Ignore differences in whether Array elements are stored densely.
                if (Array_isArray(a)) {
                    na.sort();
                    nb.sort();
                }

                for (var i = 0; i < na.length; i++) {
                    var name = na[i];
                    if (name !== nb[i])
                        failPropList(na, nb, msg);
                    var da = Object_getOwnPropertyDescriptor(a, name),
                        db = Object_getOwnPropertyDescriptor(b, name);
                    var pmsg = at(msg, /^[_$A-Za-z0-9]+$/.test(name)
                                       ? /0|[1-9][0-9]*/.test(name) ? "[" + name + "]" : "." + name
                                       : "[" + uneval_(name) + "]");
                    assertSameValue(da.configurable, db.configurable, at(pmsg, ".[[Configurable]]"));
                    assertSameValue(da.enumerable, db.enumerable, at(pmsg, ".[[Enumerable]]"));
                    if (Object_hasOwnProperty(da, "value")) {
                        if (!Object_hasOwnProperty(db, "value"))
                            throw Error_("got data property, expected accessor property" + pmsg);
                        check(da.value, db.value, pmsg);
                    } else {
                        if (Object_hasOwnProperty(db, "value"))
                            throw Error_("got accessor property, expected data property" + pmsg);
                        check(da.get, db.get, at(pmsg, ".[[Get]]"));
                        check(da.set, db.set, at(pmsg, ".[[Set]]"));
                    }
                }
            };

            var ab = new Map_();
            var bpath = new Map_();

            function check(a, b, path) {
                if (typeof a === "symbol") {
                    // Symbols are primitives, but they have identity.
                    // Symbol("x") !== Symbol("x") but
                    // assertDeepEq(Symbol("x"), Symbol("x")) should pass.
                    if (typeof b !== "symbol") {
                        throw Error_("got " + uneval_(a) + ", expected " + uneval_(b) + " " + path);
                    } else if (uneval_(a) !== uneval_(b)) {
                        // We lamely use uneval_ to distinguish well-known symbols
                        // from user-created symbols. The standard doesn't offer
                        // a convenient way to do it.
                        throw Error_("got " + uneval_(a) + ", expected " + uneval_(b) + " " + path);
                    } else if (Map_has(ab, a)) {
                        assertSameValue(Map_get(ab, a), b, path);
                    } else if (Map_has(bpath, b)) {
                        var bPrevPath = Map_get(bpath, b) || "_";
                        throw Error_("got distinct symbols " + at(path, "") + " and " +
                                     at(bPrevPath, "") + ", expected the same symbol both places");
                    } else {
                        Map_set(ab, a, b);
                        Map_set(bpath, b, path);
                    }
                } else if (isPrimitive(a)) {
                    assertSameValue(a, b, path);
                } else if (isPrimitive(b)) {
                    throw Error_("got " + Object_toString(a) + ", expected " + uneval_(b) + " " + path);
                } else if (Map_has(ab, a)) {
                    assertSameValue(Map_get(ab, a), b, path);
                } else if (Map_has(bpath, b)) {
                    var bPrevPath = Map_get(bpath, b) || "_";
                    throw Error_("got distinct objects " + at(path, "") + " and " + at(bPrevPath, "") +
                                 ", expected the same object both places");
                } else {
                    Map_set(ab, a, b);
                    Map_set(bpath, b, path);
                    if (a !== b || strictEquivalence) {
                        assertSameClass(a, b, path);
                        assertSameProto(a, b, path);
                        assertSameProps(a, b, path);
                        assertSameValue(Object_isExtensible(a),
                                        Object_isExtensible(b),
                                        at(path, ".[[Extensible]]"));
                    }
                }
            }

            check(a, b, "");
        };
    })();
}

if (typeof stringifyTaint === 'undefined') {
    // Produce a string representation of the provided taint information.
    var stringifyTaint = function(taint) {
        function replacer(key, value) {
            if (key == 'flow') {
                return undefined;
            }
            return value;
        }
        return JSON.stringify(taint, replacer);
    }
}

if (typeof assertTainted === 'undefined') {
    // Assert that at least part of the given string is tainted.
    var assertTainted = function (str) {
        if (str.taint.length == 0) {
            throw Error("String ('" + str + "') is not tainted");
        }
    }
}

if (typeof assertRangeTainted === 'undefined') {
    // Assert that the given range is fully tainted in the provided string.
    var assertRangeTainted = function(str, range) {
        function stringifyRange(range) {
            return "[" + range[0] + ", " + (range[1] === STR_END ? "str.length" : range[1]) + "]";
        }

        var begin = range[0];
        var end = range[1] === STR_END ? str.length : range[1];

        if (begin === end)
            return;

        if (begin > end)
            throw Error("Invalid range: " + stringifyRange(range));

        var curBegin = 0;
        var curEnd = 0;
        for (var i = 0; i < str.taint.length; i++) {
            var curRange = str.taint[i];
            if (curRange.begin == curEnd) {
                // Extend current range
                curEnd = curRange.end;
            } else {
                if (begin < curRange.begin)
                    // If the target range is tainted then it must lie inside the current range
                    break;

                // Start a new range
                curBegin = curRange.begin;
                curEnd = curRange.end;
            }
        }

        if (!(begin >= curBegin && begin < curEnd && end <= curEnd))
            // Target range not included in current range
            throw Error("Range " + stringifyRange(range) + " not tainted in string '" + str + "'");
    }
}

if (typeof assertRangesTainted === 'undefined') {
    // Assert that the provided ranges (varargs arguments) are tainted.
    // Example usage: assertRangesTainted(str, [0,3], [5, 8], [10, STR_END]);
    var assertRangesTainted = function(str) {
        var ranges = arguments;
        for (var i = 1; i < ranges.length; i++) {
            // This maybe isn't super effecient...
            assertRangeTainted(str, ranges[i]);
        }
    }
}

if (typeof assertFullTainted === 'undefined') {
    // Assert that every character of the given string is tainted.
    var assertFullTainted = function(str) {
        assertRangeTainted(str, [0, STR_END]);
    }
}

if (typeof assertNotTainted === 'undefined') {
    // Assert that the given string is not tainted.
    var assertNotTainted = function(str) {
        if (str.taint.length != 0) {
            throw Error("String ('" + str + "') is tainted");
        }
    }
}

if (typeof assertEqualTaint === 'undefined') {
    // Assert that the given strings are equally tainted. This only compares the ranges, not the operations.
    var assertEqualTaint = function(s1, s2) {
        var t1 = s1.taint;
        var t2 = s2.taint;

        if (s1.length != s2.length || t1.length != t2.length) {
            throw Error("The argument strings are not equally tainted (length mismatch): " + stringifyTaint(t1) + " vs " + stringifyTaint(t2));
        }

        for (var i = 0; i < t1.length; i++) {
            if (t1[i].begin != t2[i].begin || t1[i].end != t2[i].end) {
                throw Error("The argument strings are not equally tainted: " + stringifyTaint(t1) + " vs " + stringifyTaint(t2));
            }
        }
    }
}

if (typeof rand === 'undefined') {
    // Return a random integer in the range [start, end)
    var rand = function(start, end) {
        return Math.floor(Math.random() * (end - start)) + start;       // Minimum length of 4, for multiTaint to be usable
    }
}

if (typeof randomString === 'undefined') {
    // Generate a random string
    var randomString = function(len) {
        if (len === undefined)
            len = rand(4, 25);       // Minimum length of 4, for multiTaint to be usable

        var str = "";
        var charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789   \n{}[]()!@#$%^&*-_=+'\";:/?.,<>";   // TODO extend

        for (var i = 0; i < len; i++)
            str += charset.charAt(Math.floor(Math.random() * charset.length));

        return str;
    }
}

if (typeof randomTaintedString === 'undefined') {
    // Generate a random tainted string
    var randomTaintedString = function() {
        return taint(randomString());
    }
}

if (typeof multiTaint === 'undefined') {
    // Taint a random number of substrings of the given string
    var multiTaint = function(str) {
        var last_index = 0;
        var parts = [];
        while (last_index < str.length - 3) {
            var start_index = rand(last_index, Math.min(last_index + 5, str.length - 1));
            var end_index = rand(start_index + 1, Math.min(start_index + 5, str.length));
            parts.push(str.substr(last_index, start_index));
            parts.push(taint(str.substr(start_index, end_index)));
            last_index = end_index;
        }
        parts.push(str.substr(last_index, str.length));
        print(parts);
        return parts.join('');
    }
}

if (typeof randomMultiTaintedString === 'undefined') {
    // Generates a random string with randomly tainted substrings
    var randomMultiTaintedString = function(len) {
        var str = randomString(len);
        return multiTaint(str);
    }
}

if (typeof assertHasTaintOperation === 'undefined') {
    var assertHasTaintOperation = function(str, opName) {
        for (var i = 0; i < str.taint.length; i++) {
            var range = str.taint[i];
            for (var j = 0; j < range.flow.length; j++) {
                var node = range.flow[j];
                if (node.operation === opName) {
                    return true;
                }
            }
        }
        return false;
    }
}

if (typeof runTaintTest === 'undefined') {
    // Run the given tests in interpreter and JIT mode.
    var runTaintTest = function(doTest) {
        // Separate function so it's visible in the backtrace
        var runJITTest = function(doTest) {
            // Force JIT compilation
            for (var i = 0; i < 500; i++) {
                doTest();
            }
        }

        doTest();         // Will be interpreted
        runJITTest(doTest);
    }
}
