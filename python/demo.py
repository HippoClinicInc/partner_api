import os
import json
# pip install requests boto3
import requests
import boto3
ENV = "https://hippoclinic.com"
S3_BUCKET = "hippoclinic-staging"
AUTHORIZATION_PREFIX = "Bearer "
# You need to change to your account when testing this.
LOGIN_ACCOUNT = "2546566177@qq.com"
LOGIN_ACCOUNT_PASSWORD = "u3LJ2lXv"

# The MRN of the patient to upload the file to.
PATIENT_MRN = 121371837
PATIENT_NAME = "Test api"

if __name__ == "__main__":
    # Preparation: You need to enter the file path required for the upload in step 5.
    upload_local_file_path = input("Please enter the file path to upload: ").strip()
    if not os.path.exists(upload_local_file_path):
        raise FileNotFoundError(f"Path does not exist: {upload_local_file_path}")

    # Step 1: Log in to your account and obtain the token for subsequent API access.
    login_account_api_path = ENV + "/hippo/thirdParty/user/login"
    payload = json.dumps({
        "userMessage": {"email": LOGIN_ACCOUNT},
        "password": LOGIN_ACCOUNT_PASSWORD
    })
    headers = {"Content-Type": "application/json"}
    login_response = requests.request("POST", login_account_api_path, headers=headers, data=payload)
    print(f"login_account_api response.text: {login_response.text}")
    login_response_data = json.loads(login_response.text)["data"]
    jwt_token = login_response_data["jwtToken"]
    hospital_id = login_response_data["userInfo"]["hospitalId"]
    print(f"jwt_token: {jwt_token}")
    print(f"hospital_id: {hospital_id}")

    # Step 2 (optional): If the patient has not been created, you can call the Create Patient API to obtain
    # the patientId for subsequent uploaded files.
    create_patient_api_path = ENV + "/hippo/thirdParty/queryOrCreateNatusPatient"
    authorization_header = AUTHORIZATION_PREFIX + jwt_token
    payload = json.dumps({
        "hospitalId": hospital_id,
        "user": {
            "name": PATIENT_NAME,
            "mrn": PATIENT_MRN,
            # Patient role enumeration value.
            "roles": [3],
            "hospitalId": hospital_id
        }
    })
    headers = {
        'Authorization': authorization_header,
        'Content-Type': 'application/json'
    }
    create_patient_response = requests.request("POST", create_patient_api_path, headers=headers, data=payload)
    print(f"create_patient response.text: {create_patient_response.text}")
    create_patient_response_data_json = json.loads(create_patient_response.text)["data"]
    patient_id = create_patient_response_data_json["patientId"]
    print(f"patient_id: {patient_id}")

    # Step 3: Get the S3 credentials uploaded to S3 by specifying the patientId.
    get_s3_credentials_api_path = ENV + "/hippo/thirdParty/file/getS3Credentials"
    payload = json.dumps({
        "keyId": patient_id,
        # Indicates obtaining credentials to access a patient folder.
        "resourceType": 2
    })
    headers = {
        'Authorization': AUTHORIZATION_PREFIX + jwt_token,
        'Content-Type': 'application/json'
    }
    s3_credential_response = requests.request("POST", get_s3_credentials_api_path, headers=headers, data=payload)
    print(s3_credential_response.text)
    s3_credential_json = json.loads(s3_credential_response.text)["data"]
    # Each API request uses a credential that only accesses the requested patient folder.
    # The expiration time is determined by the expirationTimestampSecondsInUTC value returned by the API.
    # If the current timestamp is greater than expirationTimestampSecondsInUTC, the credential has expired.
    s3_credential = s3_credential_json["amazonTemporaryCredentials"]
    s3_credential_expiration_timestamp = s3_credential["expirationTimestampSecondsInUTC"]
    print(f"s3_credential_expiration_timestamp: {s3_credential_expiration_timestamp}")

    # Step 4: Request to generate a unique dataId for splicing the S3 file key of the uploaded file.
    generate_data_id_api_path = ENV + "/hippo/thirdParty/file/generateUniqueKey/1"
    payload = {}
    headers = {
        'Authorization': AUTHORIZATION_PREFIX + jwt_token,
    }
    generate_data_id_response = requests.request("GET", generate_data_id_api_path, headers=headers, data=payload)
    print(generate_data_id_response.text)
    generate_data_id_response_data_json = json.loads(generate_data_id_response.text)["data"]
    data_id = generate_data_id_response_data_json["keys"][0]
    print(f"generate data_id: {data_id}")

    # Step 5: Upload the file to S3 using the obtained file key and S3 credentials.
    s3_upload_file_key = f"patient/{patient_id}/source_data/{data_id}/"
    s3_client = boto3.client(
        "s3",
        aws_access_key_id=s3_credential["accessKeyId"],
        aws_secret_access_key=s3_credential["secretAccessKey"],
        aws_session_token=s3_credential["sessionToken"],
    )

    upload_file_size_bytes = 0
    upload_file_data_name = ""
    if os.path.isfile(upload_local_file_path):
        file_name = os.path.basename(upload_local_file_path)
        upload_file_data_name = file_name
        s3_key = os.path.join(s3_upload_file_key, file_name).replace("\\", "/")
        upload_file_size_bytes = os.path.getsize(upload_local_file_path)
        s3_client.upload_file(upload_local_file_path, S3_BUCKET, s3_key)
        print(f"File uploaded to s3://{S3_BUCKET}/{s3_key}")

    elif os.path.isdir(upload_local_file_path):
        folder_name = os.path.basename(os.path.normpath(upload_local_file_path))
        upload_file_data_name = folder_name
        base_prefix = os.path.join(s3_upload_file_key, folder_name).replace("\\", "/")

        for root, dirs, files in os.walk(upload_local_file_path):
            for file in files:
                local_file_path = os.path.join(root, file)
                relative_path = os.path.relpath(local_file_path, upload_local_file_path)
                s3_key = os.path.join(base_prefix, relative_path).replace("\\", "/")
                file_size = os.path.getsize(local_file_path)
                upload_file_size_bytes += file_size
                s3_client.upload_file(local_file_path, S3_BUCKET, s3_key)
                print(f"Uploaded {local_file_path} to s3://{S3_BUCKET}/{s3_key}")
    else:
        raise ValueError(f"{upload_local_file_path} is neither a file nor a folder")

    # Step 6: After the upload is successful, you need to call back the hippoclinic server to tell it
    # to upload the raw file data.
    confirm_upload_raw_file_api_path = ENV + "/hippo/thirdParty/file/confirmUploadRawFile"
    payload = json.dumps({
        "dataId": data_id,
        "dataName": upload_file_data_name,
        "fileName": s3_upload_file_key,
        "dataSize": upload_file_size_bytes,
        "patientId": patient_id,
        # EEG_RAW file type enumeration value.
        "dataType": 20,
        "uploadDataName": upload_file_data_name,
        "isRawDataInternal": 1,
        "dataVersions": [0]
    })
    headers = {
        'Authorization': AUTHORIZATION_PREFIX + jwt_token,
        'Content-Type': 'application/json'
    }
    response = requests.request("POST", confirm_upload_raw_file_api_path, headers=headers, data=payload)
    print(response.text)

    # Step 7. After 5 minutes, you can log in to the dev website to check.
    print(f"The upload process is complete. Wait about 5 minutes. "
          f"You can check it on the workspace page: {ENV}/patient/{patient_id}/data "
          f"or the detailed file page: {ENV}/signal/{data_id}")
