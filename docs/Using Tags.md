# Using Tags

HART supports tags, which will let you run only test cases marked with specific tags. To make a test case with tags, use the `HART_TEST_WITH_TAGS()` and `HART_GENERATE_WITH_TAGS()` macros, instead of their tag-less counterparts:

```cpp
HART_TEST_WITH_TAGS ("Tests name", "[tag][other-tag][some-other-tag]")
{
	// ...
}
```

To run only tasks marked with specific tags, run your test target with `--tags` (or `-t`) argument:

```bash
./YourAudioTestTarget --tags "[tag][other-tag][some-other-tag]"
```

Each task ("test" or "generate") has a set of tags, and your CLI-provided `--tags` value is also treated as a set of tags. If the intersection of those two sets is not empty, the task will run. If you didn't supply any tags via CLI argument, all tasks will run. If a task has no tags, but you've supplied at least one tag via CLI argument, that task will not run. This is it. There are no advanced binary operations on tags, like in Catch2.

Tags are put into square brackets. Inside those brackets you can use any characters, except for square brackets. So you're not just limited to letters, digits and dashes, although you can only use ASCII characters. Also, tags are case-sensitive.
