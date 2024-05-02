#!/usr/bin/env python3

import pygsheets 
import pandas as pd
from collections import defaultdict
from math import isnan


client = pygsheets.authorize(service_account_file='../attendance/key_2024') 
# opens a spreadsheet by its name/title

raw_sheet = client.open("Spring 2024 Attendance")

sheet_names = [s.title for s in raw_sheet.worksheets()]

# opens a worksheet by its name/title 
zero_worksht = raw_sheet.worksheet("title", sheet_names[0]) 
one_worksht = raw_sheet.worksheet("title", sheet_names[1]) 
two_worksht = raw_sheet.worksheet("title", sheet_names[2])


data = zero_worksht.get_all_values()

headers = data.pop(0)
df = pd.DataFrame(data, columns=headers)
df.rename(columns={"Attendees\nEnter as a list separated by line returns (no bullet points)": "Attendees"}, inplace=True)
nan_value = float("NaN") 
 
df.replace("", nan_value, inplace=True) 
df.dropna(how='all', axis=1, inplace=True) 
df.dropna(how='all', axis=0, inplace=True)
columns = df.columns.to_list()

# create dict with all of the practices everybody has attended 
values = defaultdict(set)
for column in columns:
    names = df[column].to_list()
    for name in names:
        if type(name) == float: continue
        values[name.lower()].add(column)

# update values starting at A1
combined_data = [["Full-Name", "Practices-Attended"]]
combined_data.extend(sorted([[names.title(), len(dates)] for names, dates in values.items()], key=lambda x : x[0].split(" ")[-1]))
one_worksht.clear()
one_worksht.update_values('A1', combined_data)


date_df = pd.DataFrame(columns=["Name"] + columns)

# fill new dataframe with present structure
for name in values.keys():
    temp = {"Name" : name.title()}
    for column in columns:
        if column in values[name]:
            temp[column] = "Present"
        else:
            temp[column] = "Absent"
        
    new = pd.DataFrame([temp], index=[len(date_df)])
    date_df = pd.concat([date_df, new], ignore_index=True)


# send to the worksheet
two_worksht.clear()
two_worksht.set_dataframe(date_df, start="A1")