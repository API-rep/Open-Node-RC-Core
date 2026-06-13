### Step Documentation

When step comments are used, the Doxygen description should briefly explain the purpose of each major step.
Sub-steps only need documentation when their intent is not obvious.
Step numbering in `@details` must reflect the numbering of `// N.` comments in the function body.
Reusable libraries and non-trivial logic should provide rich `@details`
Prefer concise comments for obvious code and detailed notes for non-trivial behaviors.

+++
Header Documentation (.h)

Header files should begin with a file-level Doxygen block.

Public declarations use /// comments.

Member fields use ///< inline comments.

+++
Source Documentation (.cpp)

Non-trivial functions should be documented using Doxygen blocks describing their purpose and execution flow.

Trivial implementations may rely on the documentation provided in the corresponding header
