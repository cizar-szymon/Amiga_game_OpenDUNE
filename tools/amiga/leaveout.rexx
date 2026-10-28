/* Leave the OpenDUNE icon out on the Workbench desktop */
OPTIONS RESULTS
ADDRESS WORKBENCH
'WINDOW "WorkSD:Games/OpenDUNEWarp" OPEN'
SAY 'open' rc
'ICON WINDOW "WorkSD:Games/OpenDUNEWarp" NAMES OpenDUNE SELECT'
SAY 'select' rc
'MENU WINDOW "WorkSD:Games/OpenDUNEWarp" INVOKE ICONS.LEAVEOUT'
SAY 'leaveout' rc
'WINDOW "WorkSD:Games/OpenDUNEWarp" CLOSE'
SAY 'close' rc
