#include <salts/plugin.h>

static const cmeta_plugin_manifest book_plugin_manifest = {
    .struct_size = CMETA_PLUGIN_MANIFEST_SIZE,
    .abi_version = CMETA_PLUGIN_ABI_VERSION,
    .plugin_id = "book.plugin",
    .version = { 1u, 0u, 0u },
    .exports = NULL,
    .export_count = 0u,
    .self = NULL,
    .start = NULL,
    .request_stop = NULL,
    .is_quiescent = NULL,
    .destroy = NULL
};

static const cmeta_plugin_manifest *
book_plugin_query(uint32_t host_abi)
{
    return host_abi == CMETA_PLUGIN_ABI_VERSION
        ? &book_plugin_manifest
        : NULL;
}

int main(void)
{
    cmeta_plugin_manifest wrong;
    const cmeta_plugin_manifest *published;

    published = book_plugin_query(CMETA_PLUGIN_ABI_VERSION);
    if (published != &book_plugin_manifest)
        return 1;

    if (cmeta_plugin_manifest_validate(published) != CMETA_PLUGIN_OK)
        return 2;

    if (book_plugin_query(CMETA_PLUGIN_ABI_VERSION + 1u) != NULL)
        return 3;

    wrong = book_plugin_manifest;
    wrong.abi_version = CMETA_PLUGIN_ABI_VERSION + 1u;
    if (cmeta_plugin_manifest_validate(&wrong) != CMETA_PLUGIN_UNSUPPORTED_ABI)
        return 4;

    wrong = book_plugin_manifest;
    wrong.struct_size = CMETA_PLUGIN_MANIFEST_SIZE - 1u;
    if (cmeta_plugin_manifest_validate(&wrong) != CMETA_PLUGIN_INVALID_MANIFEST)
        return 5;

    return 0;
}
