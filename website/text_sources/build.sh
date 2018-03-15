pandoc -o text.html text.md
cat template.html text.html template_end.html > ../index.html
