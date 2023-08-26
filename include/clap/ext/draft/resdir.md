
# Resource Directories

This extension provides a way for a plugin to store (collect) a snapshot of
its resources as files in a writable resource directory provided by the host and
recover the resources later on from a read-only resource directory
provided by the host.

## Requirements

Paths stored in the plugin state **must** be relative, using the resource directory as root.

The resource recovery directory **must not** be written to, the directory
and its contents may reside on read-only media in certain implementations.

The directory to collect resources into must **only** be written to during collection,
it may be removed afterwards by the host. However host implementations may only
remove such a directory **after** the next call that provides
a new resource recovery directory.

The collect directory and the contents of files stored under the collect directory
must only be written to during `clap_plugin_resource_directory->collect()` function calls.
The plugin may create symlinks in the collect directory that point to files stored
outside the collect directory for which this restriction does not apply.

The host asks plugins to store references to all required resources in a writable
directory during `clap_plugin_resource_directory->collect()`.
The plugin may be active and processing during calls to `collect()`.
This directory must not be written to after the call to `collect()` returns.
Plugins are encouraged to create symlinks from this directory to other places in
the filesystem or to use efficient copies (copy-on-write) where possible.

The host provides plugins with a read-only recovery directory with a call to
`clap_plugin_resource_directory->set_directory()` which contains resources
previously collected or links to previously collected resources.
This directory and its contents stay valid until after the next call to
`clap_plugin_resource_directory->set_directory()` is completed or until after
a plugin is destroyed.
This allows plugins to reference recovery directory resources from concurrent
threads (e.g. in `clap_plugin->process()`) until its `set_directory()` implementation
synchronized a resource switch.

Host implementations may deduplicate resources across different plugin resource directories
using cryptographically secure content hashes, in which case a file that was copied into
the collect directory can show up as a linked file in a recovery directory at a later point.

## Use Cases

Read-only Recovery:
- The recover directory may reside on read-only meadia (like a CD-Rom ISO) or
  in a non-writable hierarchy (/usr/share/ on Unix), so temporary files and files for live editing
  need to be managed in (or copied into) other locations by plugins (such as a `/tmp/` directory).

Collect Live Buffer:
- A plugin might use e.g. a `/tmp/waveshaping.bin` file for live edits.
  Upon `collect()`, it creates a symlink to the `/tmp/waveshaping.bin` file from the resource directory.

Collect Factory Resource File:
- A plugin creates a symlink to the factory file from the resource
  directory during `collect()`.

Collect Factory Memory Resource:
- The plugin needs to store the contents from factory memory in
  a file in the resource directory upon `collect()`. In this case, using a content hash as file name
  may simplify de-duplication during recovery for futue plugin versions.

Collect Recovered File:
- Create a symlink from the collect directory to the file in the recovery
  directory during `collect()`.

Sharing Collected Files:
- Multiple plugins can make efficient use of the same file (e.g. filter
  impulse response) by linking to the same file during `collect()` (which may be external to the
  resource directory or inside the recovery directory). De-duplication is left to host implementations
  which may lead to the file to show up as a linked file in a future recovery directory.

## Further Notes

- Garbage Collection: No GC logic between file collections from different plugins is required
  by host implementations.

- Collisions: No file name collisions can occour between plugins, because all
  plugins only write into isolated directories.

- De-duplication and COW: Most users on pre-installed systems will currently (2023) not benefit
  from a COW file system, so extensive use of symlinks is recommended instead.
  Hosts may still de-duplicate files based on content hashes.

- Read-only Limitation: Plugins need to be well functioning under hosts that do *not* implement
  this extension, so they already have to provide logic for factory fils or live buffer
  management. To implement collect, this extensions just asks for a link to those files and
  can provide a stable read-only resource file in return. So demanding read-only contents
  under the collect directory is more of a stability guarantee than a limitation.

- Flushing State: State that must be consistently flushed will have to be written into
  newly created files by plugins. Live buffers where inconsistencies may have negligible
  impact can simply be linked to during collect.

- Directory Cleanup: Hosts may turn collect directories into read-only resource
  directories by replacing symlinks in a collect directory with copies or hardlinks
  to actual content if the symlinks point into about-to-be-purged directories (e.g. old
  read-only resource directories). Actual purging of directories may happen after
  plugins could switch resources over to the new (now read-only) resource directory.

- Archival: For archival (e.g. into a ZIP cnotainer), all resources pointed to by
  symlinks linking outside of a collect directory need to be stored by following links.


## Interface

```c
typedef struct clap_plugin_resource_directory {
   // Sets the absolute path of a read-only directory from which the plugin can
   // access its resources, the plugin must not modify the directory contents
   // and needs to be able to follow links outside the directory.
   // The directory remains valid until after the plugin is destroyed or until
   // after it is overriden by another call to this function.
   // If path is null or blank, it clears the directory location.
   // [main-thread]
   void (CLAP_ABI *set_directory) (const clap_plugin_t *plugin, const char *path);

   // Asks the plugin to store a snapshot of its resources into a writable
   // directory specified via an absolute path. Use of links is recommended,
   // including links into a previously provided read-only resource directory.
   // It is not necessary to collect files which belongs to the plugin's
   // factory content unless the param all_resources is true.
   // [main-thread]
   void (CLAP_ABI *collect) (const clap_plugin_t *plugin, const char *path,
                             bool all_resources);
} clap_plugin_resource_directory_t;
```
