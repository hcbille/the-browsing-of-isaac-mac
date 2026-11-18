# Saves are imported once and never written back to Steam Cloud

The Mac Build copies the user's existing Repentance+ save into its own save folder on first launch (an Imported Save) and from then on keeps its own progress, never writing to Steam Cloud. Two-way sync was rejected because the cloud save is shared with the user's real Repentance+ on other platforms, and a bad write from an experimental build could damage real progress that cannot be recovered. An existing Mac Build save is never overwritten by a later import.
