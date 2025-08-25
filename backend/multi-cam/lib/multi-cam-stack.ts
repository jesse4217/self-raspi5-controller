import * as cdk from 'aws-cdk-lib';
import { Construct } from 'constructs';
import * as lambda from 'aws-cdk-lib/aws-lambda';

export class MultiCamStack extends cdk.Stack {
  constructor(scope: Construct, id: string, props?: cdk.StackProps) {
    super(scope, id, props);

    // const invokeRealityScanBatch = new lambda.Function(this, "InvokeRealityScanBatch", {
    //   runtime: lambda.Runtime.NODEJS_20_X, // Provide any supported Node.js runtime
    //   handler: "index.handler",
    //   code: lambda.Code.fromInline(`
    //     exports.handler = async function(event) {
    //       return {
    //         statusCode: 200,
    //         body: JSON.stringify('Hello AWS Batch!'),
    //       };
    //     };
    //   `),
    // })
    //
    // const invokeRealityScanBatchUrl = invokeRealityScanBatch.addFunctionUrl({
    //   authType: lambda.FunctionUrlAuthType.NONE,
    // });
    //
    // // Define a CloudFormation output for your URL
    // new cdk.CfnOutput(this, "invokeRealityScanBatchUrlOutput", {
    //   value: invokeRealityScanBatchUrl.url,
    // })
  }
}
