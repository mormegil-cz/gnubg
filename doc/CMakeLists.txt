install(DIRECTORY images DESTINATION ${DOCDIR} PATTERN "CVS" EXCLUDE)
install(FILES "gnubg.html" "allabout.html" DESTINATION share/doc/gnubg)
install(FILES "gnubg.pdf" "allabout.pdf" DESTINATION share/doc/gnubg)
install(FILES "gnubg.6" DESTINATION share/man/man6)

