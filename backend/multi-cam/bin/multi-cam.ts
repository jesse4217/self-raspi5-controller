#!/usr/bin/env node
import 'dotenv/config'; // Load .env file
import * as cdk from 'aws-cdk-lib';
import { MultiCamStack } from '../lib/multi-cam-stack';

const app = new cdk.App();
new MultiCamStack(app, 'MultiCamStack', {
  /* If you don't specify 'env', this stack will be environment-agnostic.
   * Account/Region-dependent features and context lookups will not work,
   * but a single synthesized template can be deployed anywhere. */

  /* Using environment variables from .env file or AWS CLI configuration */
  env: { 
    account: process.env.AWS_ACCOUNT_ID || process.env.CDK_DEFAULT_ACCOUNT, 
    region: process.env.AWS_REGION || process.env.CDK_DEFAULT_REGION 
  },

  /* Uncomment the next line if you know exactly what Account and Region you
   * want to deploy the stack to. */
  // env: { account: '123456789012', region: 'us-east-1' },

  /* For more information, see https://docs.aws.amazon.com/cdk/latest/guide/environments.html */
});
