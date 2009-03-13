install(DIRECTORY images DESTINATION ${DOCDIR} PATTERN "CVS" EXCLUDE)
install(FILES "gnubg/gnubg.html" "gnubg/allabout.html" DESTINATION share/doc/gnubg)
install(FILES "gnubg/gnubg.pdf" "gnubg/allabout.pdf" DESTINATION share/doc/gnubg)
install(FILES "gnubg/gnubg.6" DESTINATION share/man/man6)

