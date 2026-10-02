#!/usr/bin/env python3
# Must match language_engine.h order. IDs are part of VPK4 on-disk format.
LANGUAGES = [
 ('AUTO','AUTO','auto'),('ENGLISH','ENGLISH','en'),('HINDI','HINDI','hi'),('ARABIC','ARABIC','ar'),
 ('BENGALI','BENGALI','bn'),('PORTUGUESE','PORTUGUESE','pt'),('INDONESIAN','INDONESIAN','id'),('URDU','URDU','ur'),
 ('RUSSIAN','RUSSIAN','ru'),('CHINESE - MANDARIN','CHINESE_MANDARIN','cmn'),('CHINESE - CANTONESE','CHINESE_CANTONESE','yue'),
 ('SPANISH','SPANISH','es'),('FRENCH','FRENCH','fr'),('GERMAN','GERMAN','de'),('ITALIAN','ITALIAN','it'),
 ('JAPANESE','JAPANESE','ja'),('KOREAN','KOREAN','ko'),('DUTCH','DUTCH','nl'),('THAI','THAI','th'),
 ('FILIPINO / TAGALOG','FILIPINO_TAGALOG','fil'),('VIETNAMESE','VIETNAMESE','vi'),('MALAY','MALAY','ms'),
 ('TURKISH','TURKISH','tr'),('PERSIAN','PERSIAN','fa'),('PUNJABI','PUNJABI','pa'),('TAMIL','TAMIL','ta'),
 ('SWAHILI','SWAHILI','sw'),('HAUSA','HAUSA','ha'),('YORUBA','YORUBA','yo'),('IGBO','IGBO','ig'),
 ('AMHARIC','AMHARIC','am'),('OROMO','OROMO','om'),('SOMALI','SOMALI','so'),('ZULU','ZULU','zu'),
 ('XHOSA','XHOSA','xh'),('AFRIKAANS','AFRIKAANS','af'),('SHONA','SHONA','sn'),('KINYARWANDA','KINYARWANDA','rw'),
 ('MALAGASY','MALAGASY','mg'),('WOLOF','WOLOF','wo'),('LINGALA','LINGALA','ln'),('AKAN / TWI','AKAN_TWI','ak'),
 ('FULA','FULA','ff'),('TIGRINYA','TIGRINYA','ti'),('SESOTHO','SESOTHO','st'),('SETSWANA','SETSWANA','tn'),
 ('CHICHEWA','CHICHEWA','ny')]
BY_FOLDER={folder:i for i,(_,folder,_) in enumerate(LANGUAGES)}
BY_ISO={iso:i for i,(_,_,iso) in enumerate(LANGUAGES)}
