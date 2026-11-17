# NATI LAB · XENSIV Radar + BLE

ממשק מקומי בעברית לרדאר **Infineon BGT60CUTR13AIP**, על ערכת **KIT_CSK_BGT60CUTR13 / CYSBSYSKIT-DEV-01** עם PSOC 6 ו־CYW43012.

הקושחה מוסיפה BLE לדוגמת מעקב האנשים של Infineon. ב־Chrome/Edge במחשב Windows אפשר לראות מסלולים וזיהויים, לשנות הגדרות חיישן, ולהקליט דגימות ADC לניתוח בדיעבד. אין צורך בשירות ענן; הנתונים עוברים ישירות מהכרטיס לדפדפן ב־BLE.

**מצב הגרסה:** BLE ‏0.2.0 היא גרסת פיתוח. מעקב חי, הגדרות והקלטת 5 שניות נבדקו; בבדיקה חוזרת ב־17.11.2026 הורדת 10 שניות נעצרה ב־198 מתוך 204 מסגרות. התקלה אינה מתרחשת בכל הורדה, והקלטות מוצלחות נצפו גם אצל המשתמש. פירוט ההבדל בין הבניות ומה אומת מופיע ב־[KNOWN_ISSUES.md](KNOWN_ISSUES.md).

## הפעלה מהירה

1. התקן [Python](https://www.python.org/downloads/windows/) והפעל את האפשרות להוסיף אותו ל־PATH.
2. הורד או שכפל את הריפו. ספק מתח לכרטיס עם קושחת BLE מותקנת.
3. הפעל בלחיצה כפולה את **[Start-Radar.cmd](Start-Radar.cmd)**.
4. פתח `http://localhost:8765` ב־Chrome או Edge במחשב Windows עם Bluetooth פעיל.
5. לחץ **חיבור לחיישן**, בחר **Radar-CSK** ואשר את החיבור בחלון הדפדפן.
6. הגדר גובה, הטיה וכיוון TX לפי ההתקנה בפועל. לאחר החלה, הדף קורא את ההגדרות בחזרה ומאמת אותן.

אין צורך ב־npm או בבניית ממשק. השרת משתמש בספרייה הסטנדרטית של Python ומאזין רק למחשב המקומי. פותחים את הדף דרך השרת, ולא ישירות כקובץ `file://`. בחירת מכשיר BLE דורשת לחיצה של המשתמש.

הכרטיס מפעיל את הרדאר ומפרסם את עצמו לאחר אתחול. הגדרות שהוחלו זמניות עד אתחול; אפשר לייצא אותן ל־JSON ולהחזיר דרך הדף. טעינת קובץ ממלאת את הטופס, והחלה היא פעולה נפרדת. עדכון קושחה מתבצע ב־USB/KitProg3; אין BLE OTA.

## האתר בענן

כתובת: https://yellow-coast-0adb04103.4.azurestaticapps.net

GitHub Actions מפרסם רק את תיקיית `web/` ל־Azure Static Web Apps, ללא בניית Node, API או קושחה. הפריסה מופעלת ב־push לענף `main` שמשנה קובץ ב־`web/` או את קובץ הפריסה `.github/workflows/azure-static-web-apps-yellow-coast-0adb04103.yml`. שינוי בקושחה, בתמונות HEX או בתיעוד שורש בלבד אינו מפעיל פריסה. אין פריסות Preview של Pull Requests.

ה־Deployment token נשאר ב־GitHub Secret שנוצר על ידי Azure. האתר בענן מספק את הממשק וה־WIKI; חיבור BLE נעשה מהמחשב של המבקר לרדאר הקרוב אליו, לאחר בחירת החיישן בדפדפן. אין צורך ב־Start-Radar.cmd בעת שימוש בכתובת הענן.

## צריבה ושחזור בלחיצה כפולה

נדרשת התקנת [XENSIV CMOS Radar Visualization Tool 1.6.0](https://softwaretools.infineon.com/tools/com.ifx.tb.tool.xensivcmosradarvisualizationtool), הכוללת את OpenOCD המשמש כאן. **לצריבת הקבצים המוכנים אין צורך ב־ModusToolbox.**

| קובץ | פעולה |
| --- | --- |
| [Install-Radar-BLE.cmd](Install-Radar-BLE.cmd) | צריבת BLE ‏0.2.0 development, כולל פרסום BLE רציף |
| [Restore-Original.cmd](Restore-Original.cmd) | שחזור `presence_detection_and_tracking` המקורית של Infineon, גרסה 1.4.1 |
| [Start-Radar.cmd](Start-Radar.cmd) | פתיחת הממשק וה־WIKI המקומיים |

חבר USB לחיבור **KitProg3** ונתק את החיבור ב־XENSIV ובדפדפן. פתיחת קובץ הצריבה מתחילה את הפעולה מיד. השאר USB מחובר עד `SUCCESS`. הסקריפט בודק SHA-256 של התמונה, מתכנת, מבצע `verify` ומאתחל. החלון נשאר פתוח והלוג נשמר ב־`logs/` המקומית.

בדיקת הקבצים והכלים בלבד, ללא גישה לחומרה:

```bat
Restore-Original.cmd -CheckOnly
Install-Radar-BLE.cmd -CheckOnly
```

הסקריפטים מכוונים לכרטיס הפיתוח של הפרויקט: KitProg3 מספר `110D0E8300023400`. לשימוש בכרטיס אחר יש להתאים את מספר המתכנת בסקריפטים. נתיב OpenOCD המוגדר כברירת מחדל הוא תחת `C:/Infineon/Tools/XENSIV-CMOS-Radar-Visualization-Tool/1.6.0/`; אפשר להעביר למתקין `-OpenOcdRoot` עם נתיב אחר.

לאחר שחזור, המתן לפחות 5 שניות והתחבר ב־XENSIV דרך **Evalkit CSK (UART)** לפורט של הכרטיס. אם נשארה זהות קושחה ישנה בתוכנה, נתק והחזר מתח ורענן את רשימת ההתקנים. השחזור מחזיר את יישום היצרן; הוא אינו שחזור מלא של כל זיכרונות הכרטיס והגדרותיו.

## קובצי הצריבה שנשמרים בריפו

| תמונה | תפקיד | SHA-256 |
| --- | --- | --- |
| [recovery/presence_detection_and_tracking_csk_v1_4_1.hex](recovery/presence_detection_and_tracking_csk_v1_4_1.hex) | מקורית, מסלול השחזור נבדק על חומרה | `eeb8011ed0270eca709189a441f238a72993a094d24e0ce4784de2ae40270732` |
| [release/radar-ble-v0.2.0-development.hex](release/radar-ble-v0.2.0-development.hex) | גרסת הפיתוח שהמתקין בוחר | `44e5d328b9091602c6c429158c09525e8b423c577394be2b0de82ddf76c34610` |
| [release/radar-ble-v0.1.0.hex](release/radar-ble-v0.1.0.hex) | גיבוי גרסת BLE קודמת למעקב, ללא הרחבת ההקלטה | `a8ea8b95aecaeccf72ec974ef6e5681786d4e5626f66027bf4a50ba14d7eb365` |

`.gitattributes` שומר את בתי קובצי ה־HEX ללא המרת סיומות שורה, כדי שאימות SHA-256 יעבוד גם לאחר clone. פרטי הגרסה הנוכחית נמצאים ב־[release/ble-install-manifest.json](release/ble-install-manifest.json).

## הממשק וה־WIKI

לאחר פתיחת `Start-Radar.cmd`, ה־WIKI נמצא ב־`http://localhost:8765/wiki.html`, ומקושר גם מתוך הגרפים.

המדריך מסביר את המסלול משידור FMCW ב־60 GHz דרך ערבול, ADC ועיבוד ועד לתצוגה; השוואה ל־ToF; פירוש המילה **תא**; מסלולים לעומת זיהויים; פרופיל עוצמה, מפה לאורך זמן, היסטוגרמה והשוואת הקלטות. הוא כולל הדגמה אינטראקטיבית עם הקלטה אמיתית קצרה ופרק על הפורמט המועבר ב־BLE.

נקודות ירוקות הן מסלולים עם מזהה. זיהויי Macro/Micro/Static הם פלטים נוספים של האלגוריתם. הגרף מציג 0° קדימה וזווית חיובית ימינה. שינוי טווח התצוגה משנה את הציור; שינוי Field of View משנה את הגדרות האלגוריתם.

מצב המחקר שומר 64 דגימות × 3 ערוצי RX לכל מסגרת, מהצ'ירפ הממוצע הראשון של SEQ0. הדפדפן מפריד ערוצים, מסיר ממוצע מכל צ'ירפ, מפעיל חלון Hann ו־DFT, ומציג עוצמות יחסיות ב־dBFS. **ציר התאים עדיין אינו מכויל למטרים**, והמדידה אינה מדידת טמפרטורה או זיהוי סוג מתכת. פירוט: [RESEARCH.txt](RESEARCH.txt).

## בניית הקושחה

הסביבה שנבדקה: Windows, **ModusToolbox Tools Package 3.9.0**, ‏**Arm GNU 14.2.1**, Python ו־Git. התקנה רשמית: [ModusToolbox Setup](https://softwaretools.infineon.com/tools/com.ifx.tb.tool.modustoolboxsetup).

מתוך PowerShell בשורש הריפו, בבנייה הראשונה:

```powershell
./scripts/build-ble.ps1 -GetLibraries
```

לאחר מכן:

```powershell
./scripts/build-ble.ps1
```

הסקריפט מצפה ל־ModusToolbox תחת `%USERPROFILE%/ModusToolbox/tools_3.9`. הספריות יורדות לפי `deps/*.mtb`, נעילות `deps/assetlocks.json` ותלויות ה־BSP. תיקיות `mtb_shared`, ‏`libs` ו־`build` נוצרות מקומית ואינן בריפו.

`scripts/patch-btstack.py` מופעל אוטומטית לפני הבנייה: הוא מתקן מסלול כשל בהקצאת מאגר שידור שהשאיר פסיקות כבויות. יש לשמור אותו גם לאחר עדכון/הורדת תלויות. `btstack-integration` נעולה ל־`0ce7133b380ab7b8bc5309a7902c65a520783c9e`.

התוצר יופיע ב־`firmware/radar-app/build/APP_CYSBSYSKIT-DEV-01/Release/presence_detection_and_tracking_ble.hex`. הסקריפט `scripts/flash-ble.ps1` צורב **תוצר בנייה זה**; `Install-Radar-BLE.cmd` צורב את העותק הקבוע שבתיקיית `release`.

## בדיקות

בדיקות תוכנה ללא כרטיס, עם Node.js מודרני:

```powershell
node --test tests/protocol.test.mjs tests/research.test.mjs
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/flash-launchers.test.ps1
```

בדיקות המתקינים משתמשות במתכנת מדומה ומכסות הצלחה, כשל תכנות, היעדר אישור verify, בדיקת קבצים בלבד ותמונה ששונתה. הן אינן צורבות חומרה.

כלי בדיקה אופציונליים לחומרה: `scripts/probe_serial.py`, ‏`probe_ble.py`, ‏`probe_research.py`; עבורם התקן `python -m pip install -r requirements-dev.txt`. יש לנתק תוכנות אחרות מהחיבור לפני הרצתם. `probe_research.py` מפעיל הקלטה חדשה, ו־`probe_ble.py --roundtrip` משנה זמנית הגדרה ומשחזר אותה. `check-recovery.py` מיועד לעדויות הגיבוי המקומיות בלבד, שאינן בריפו.

## מבנה ורישיונות

- `firmware/radar-app/` — קוד הקושחה, BSP, הגדרות ותוצרים נדרשים של Configurator.
- `web/` — ממשק BLE, ניתוח הנתונים ו־WIKI.
- `scripts/`, `tests/` — בנייה, צריבה, שרת מקומי ובדיקות.
- `release/`, `recovery/` — תמונות צריבה שמורות והגדרות היצרן.
- `licenses/`, [THIRD_PARTY.md](THIRD_PARTY.md) — מקורות ורישיונות רכיבי Infineon וצד שלישי. רישיונות היצרן, לרבות תנאי Evaluation הרלוונטיים, נשארים בתוקף; אין כאן רישוי מחדש שלהם.

לוגים, היסטוריית עבודה, גיבויי זיכרון מלאים, עותקי מקורות לעיון, מתקיני תוכנה ותוצרי בנייה אינם מנוהלים ב־Git. דגימות ב־`tests/fixtures/` וב־`web/wiki-capture.json` נשמרות במכוון להפעלת הבדיקות והמדריך.
