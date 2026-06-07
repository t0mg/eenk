# Hyphenation Module Attribution

This module implements the Liang hyphenation algorithm based on code from:

- **crosspoint-reader** - Liang algorithm implementation
  https://github.com/crosspoint-reader/

- **Typst hypher** - Binary trie patterns from TeX hyphenation dictionaries
  https://github.com/typst/hypher

The binary trie patterns are generated from TeX hyphenation patterns and are
subject to their respective TeX pattern licenses. See the typst/hypher
repository for individual pattern attributions.

## Supported Languages

| Language | Code | Pattern Source   |
|----------|------|------------------|
| English  | en   | hyphen.english   |
| French   | fr   | hyphen.french    |
| German   | de   | hyphen.german    |
| Spanish  | es   | hyphen.spanish   |
| Italian  | it   | hyphen.italian   |
| Ukrainian| uk   | hyphen.ukrainian |
| Russian  | ru   | hyphen.russian   |
