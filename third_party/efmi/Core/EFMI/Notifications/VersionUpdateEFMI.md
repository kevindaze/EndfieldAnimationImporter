Compatibility Warning

Starting with XXMI DLL 0.8.5, the Region Hashes system has changed in a non-backwards compatible way:

- All [TextureOverride] sections using IB hashes must now include a `match_index_count` value.

Use an appearance version compatible with the EFMI runtime bundled with EAI.
Check the appearance author's compatibility instructions.

This change prevents overly broad hash matching to reduce CPU usage peaks by up to 100 times.

For mod creators: You can now view the index count of the selected IB in Shader Hunting Mode.

Press F10 to hide this message