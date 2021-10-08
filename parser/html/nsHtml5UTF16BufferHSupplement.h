/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Modifications Copyright SAP SE. 2019-2021.  All rights reserved.
 */

protected:
nsHtml5UTF16Buffer(char16_t* aBuffer, const StringTaint& taint, int32_t aEnd);
~nsHtml5UTF16Buffer();

/**
 * For working around the privacy of |buffer| in the generated code.
 */
void DeleteBuffer();

/**
 * For working around the privacy of |buffer| in the generated code.
 */
void Swap(nsHtml5UTF16Buffer* aOther);
