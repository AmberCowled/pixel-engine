# Skill: /pr

Automates the workflow of committing local changes, pushing them to the remote repository, and creating a pull request with a detailed changelog.

## Instructions for the Agent

When this command is invoked, perform the following steps:

1. **Inspect Workspace Status**:
   - Run `git status` to see all unstaged modifications and untracked files.
   - Identify the current branch name.

2. **Stage and Commit Changes**:
   - Stage all relevant modified and new files using `git add .` (ensuring build folders, system files, and logs are ignored).
   - Generate a concise and meaningful commit message summarizing the changes.
   - Run `git commit -m "<Commit Message>"` to commit the changes.

3. **Push to Remote**:
   - Push the committed changes to the origin branch by running `git push -u origin <branch_name>`.

4. **Formulate Pull Request Details**:
   - Generate a detailed PR description.
   - Summarize the key additions, bug fixes, refactorings, and changes introduced since the last pull request or main branch.
   - Format this description using clean GitHub Markdown.

5. **Create the Pull Request**:
   - Run `gh auth status` to check if the GitHub CLI is available and authenticated.
   - **If GitHub CLI is authenticated**:
     - Run `gh pr create --title "<Title>" --body "<Body>"` to automatically create the pull request.
   - **If GitHub CLI is NOT authenticated / NOT installed**:
     - Output the detailed Pull Request description directly to the chat interface.
     - Guide the user to manually create the PR on GitHub using the provided description.
