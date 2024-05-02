#!/usr/bin/env python3
import pygsheets 
import pandas as pd

nan_value = float("NaN")
client = pygsheets.authorize(service_account_file='../attendance/key_2024') 
# opens a spreadsheet by its name/title
raw_sheet = client.open("Dues")
att_sheet = client.open("Spring 2024 Attendance")

def make_df(wksht):
    #create pandas dataframe from worksheet and set column names:
    data = wksht.get_all_values()
    headers = data.pop(0)
    df = pd.DataFrame(data, columns=headers)
    #replace empty boxes with nan_value 
    df.replace("", nan_value, inplace=True) 
    #delete columns with no values
    df.dropna(how='all', axis=1, inplace=True) 
    #delete rows with no values
    df.dropna(how='all', axis=0, inplace=True) 
    return df

def merge_names(df):
    #change first and last name column to be single full name column:
    df["Full-Name"] = df["First-Name"] + " " + df["Last-Name"]
    df.drop(['First-Name', 'Last-Name'], axis=1, inplace=True)
    #drop duplicates in the full name column:
    df.drop_duplicates(subset=['Full-Name'])
    #make the full name column all lowercase
    df['Full-Name'] = df['Full-Name'].str.lower()
    return df

#retreives a list of the names of individual worksheets
sheet_names = [s.title for s in raw_sheet.worksheets()]

# opens a worksheet by its name/title 
paid_wksht = raw_sheet.worksheet("title", "Paid") 
trips_wksht = raw_sheet.worksheet("title", "Trips") 
master_wksht = raw_sheet.worksheet("title", "Master") 
att_wksht = att_sheet.worksheet("title", "totals") 

#creates pandas dataframe with no completely empty rows or columns
paid_df = make_df(paid_wksht)
trips_df = make_df(trips_wksht)
master_df = make_df(master_wksht)
att_df = make_df(att_wksht)
#makes column Full-Name and deletes old name columns:
paid_df = merge_names(paid_df)
trips_df = merge_names(trips_df)
#complete full merge between paid_df and trips_df
merged_df = paid_df.merge(trips_df, on='Full-Name', how='outer')
# conditionally delete rows where age is less than or equal to 1
att_df["Practices-Attended"] = pd.to_numeric(att_df["Practices-Attended"], errors='coerce')
att_new = att_df.drop(att_df[att_df['Practices-Attended'] <= 1].index)
att_new['Full-Name'] = att_new['Full-Name'].str.lower()
merged_df = merged_df.merge(att_new, on='Full-Name', how='outer')
#convert data paid df to a dictionary {key= full-name: value= dict{Amount-Paid:$}}
merged_df["Red Trips (not lead)"] = pd.to_numeric(merged_df["Red Trips (not lead)"], errors='coerce')

#changes NaN values to appropriate values based on column:
merged_df["Amount-Paid"].replace(nan_value, "$0.00", inplace=True)
merged_df["Red Trips (not lead)"].replace(nan_value, 0, inplace=True) 
merged_df["First Trip"].replace(nan_value, "Not on Trip", inplace=True) 
merged_df["Second Trip"].replace(nan_value, "Not on Trip", inplace=True)
merged_df["Third Trip"].replace(nan_value, "Not on Trip", inplace=True)  
merged_df["Practices-Attended"].replace(nan_value, 0, inplace=True)
#adjust data to calculate amount owed 
merged_df['Amount-Paid'] = merged_df['Amount-Paid'].map(lambda x: x.lstrip('$').rstrip('aAbBcC'))
merged_df["Amount-Paid"] = pd.to_numeric(merged_df["Amount-Paid"], errors='coerce')
merged_df["Owed"] = (merged_df["Red Trips (not lead)"] * 30 + 10) - merged_df["Amount-Paid"]
merged_df.loc[merged_df.Owed < 0, 'Owed'] = 0
merged_df['Full-Name'] = merged_df['Full-Name'].str.title()
merged_df = merged_df.sort_values(by=['Full-Name'])
print(merged_df.to_string())

# send to the worksheet
master_wksht.clear()
master_wksht.set_dataframe(merged_df[['Full-Name', 'Owed', 'Amount-Paid', 'Practices-Attended', 'First Trip', 'Second Trip', 'Third Trip']], start="A1")