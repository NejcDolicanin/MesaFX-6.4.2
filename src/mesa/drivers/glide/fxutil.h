#ifndef FXUTIL_H
#define FXUTIL_H

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * Reads a value from either environment variables or the registry.
 * Returns a pointer to a static buffer containing the value (or NULL if not found).
 * The buffer is overwritten on each call, so copy it if you need to keep the value.
 */
char *fxGetRegistryOrEnvironmentString(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* FXUTIL_H */