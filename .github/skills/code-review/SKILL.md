---
name: code-review
description: "Perform a structured, severity-ranked review of changed files or a PR diff. Use when asked to review code, check a diff, audit a PR, or before merging. Covers correctness, security, TypeScript/React patterns, API/data layer, and tests."
---

# Code Review

Act as a senior reviewer. Be terse and specific. Every comment states what is
wrong, why it matters, and the fix.

## Scope

1. Get the diff: PR diff → `git diff --merge-base origin/main` → staged changes.
2. Review changed lines plus blast radius (callers, types, tests). Do not lint
   the whole repo.
3. Read surrounding context before commenting.
4. Ignore anything Prettier/ESLint already catches.

## Severity

- **BLOCKER** — bug, data loss, security hole, breaking API change.
- **MAJOR** — missing error handling, wrong abstraction, untested new logic.
- **MINOR** — readability, naming, duplication, dead code.
- **QUESTION** — you lack context; ask, don't assert.

## Checklist

### Correctness

- Null/undefined access, off-by-one, unhandled promise rejections.
- Floating or unawaited async; swallowed errors.
- Edge cases: empty arrays, zero, timezone/DST, unicode.
- Does it do what the PR description claims?

### Security

- Unvalidated input reaching a query, shell, filesystem path, or `eval`.
- SQL/NoSQL injection via string concatenation.
- XSS: `dangerouslySetInnerHTML`, unescaped output.
- Secrets or tokens committed or logged.
- AuthN/AuthZ on every new endpoint — verify ownership, not just login.

### TypeScript / JavaScript

- `any`, unchecked `as`, non-null `!` — demand justification.
- Mutation of props or shared objects.
- Weak types that hide invalid states; use discriminated unions.

### React

- Conditional hooks; missing/incorrect dependency arrays.
- Stale closures in `useEffect`; missing cleanup.
- Effects where derived state or an event handler would do.
- Array-index keys on reorderable lists.
- New object/array/function literals passed to memoised children.
- Accessibility: labels, roles, keyboard handlers on clickable non-buttons.

### API & data layer

- N+1 queries, missing pagination limits, missing indexes.
- Missing transactions around multi-write operations.
- Response shape changes that break consumers.

### Tests

- New behaviour has a test; bug fixes have a regression test.
- Assert behaviour, not implementation. No sleeps or shared mutable fixtures.

## Output

## Verdict: REQUEST CHANGES | APPROVE WITH COMMENTS | APPROVE

Group findings by file:

### src/api/users.ts

- **BLOCKER** L42 — `req.params.id` interpolated into the query string.
  Use `db.query('select * from users where id = $1', [id])`.

### Summary

- Counts by severity, what's genuinely good, notable test gaps.

## Rules

- Max 10 findings; if more, report the top 10 and note the pattern.
- Only report issues supported by the code. Never invent findings to fill space.
- Minimal patch-sized snippets, never full-file rewrites.
- If you can't access the diff, say so and stop.
