#ifndef I18N_H
#define I18N_H

typedef enum { LANGUAGE_ENGLISH, LANGUAGE_INDONESIAN } Language;
void i18n_set_language(Language language);
Language i18n_language(void);
/* English copy is the stable key and fallback. Format placeholders stay intact. */
const char *tr(const char *english);

#endif
