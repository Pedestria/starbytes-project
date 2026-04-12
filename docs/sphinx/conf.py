project = "Starbytes"
copyright = "2026, Starbytes"
author = "Starbytes"

version = "0.12.0"
release = "0.12.0"

extensions = [
    "sphinx.ext.autosectionlabel",
]

autosectionlabel_prefix_document = True

templates_path = []
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

source_suffix = ".rst"
master_doc = "index"
language = "en"

pygments_style = "sphinx"
highlight_language = "text"

html_theme = "starbytes_theme"
html_theme_path = ["_theme"]
html_title = "Starbytes Documentation"
html_short_title = "Starbytes Docs"
html_copy_source = False
html_show_sourcelink = False
html_last_updated_fmt = "%B %d, %Y"
html_static_path = ["_static"]
html_css_files = ["css/starbytes-theme.css"]

html_sidebars = {
    "**": [
        "searchbox.html",
        "globaltoc.html",
        "localtoc.html",
        "relations.html",
    ]
}
